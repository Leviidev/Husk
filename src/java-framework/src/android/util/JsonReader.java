package android.util;

import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.Reader;
import java.util.Arrays;

/** Reads a JSON document a token at a time (RFC 4627, with the platform's lenient extensions when asked for). */
public final class JsonReader implements Closeable {
    private static final int EMPTY_ARRAY = 1, NONEMPTY_ARRAY = 2, EMPTY_OBJECT = 3, DANGLING_NAME = 4, NONEMPTY_OBJECT = 5,
            EMPTY_DOCUMENT = 6, NONEMPTY_DOCUMENT = 7, CLOSED = 8;
    private final Reader in;
    private boolean lenient;
    private final char[] buf = new char[1024];
    private int pos, limit, line = 1, lineStart;
    private int[] stack = new int[32];
    private int depth;
    private JsonToken token;
    private String name, value;

    public JsonReader(Reader in) {
        if (in == null) throw new NullPointerException("in == null");
        this.in = in;
        push(EMPTY_DOCUMENT);
    }
    public void setLenient(boolean lenient) { this.lenient = lenient; }
    public boolean isLenient() { return lenient; }

    public void beginArray() throws IOException { expect(JsonToken.BEGIN_ARRAY); }
    public void endArray() throws IOException { expect(JsonToken.END_ARRAY); }
    public void beginObject() throws IOException { expect(JsonToken.BEGIN_OBJECT); }
    public void endObject() throws IOException { expect(JsonToken.END_OBJECT); }
    private void expect(JsonToken t) throws IOException {
        peek();
        if (token != t) throw new IllegalStateException("Expected " + t + " but was " + peek());
        advance();
    }
    public boolean hasNext() throws IOException { peek(); return token != JsonToken.END_OBJECT && token != JsonToken.END_ARRAY; }

    public JsonToken peek() throws IOException {
        if (token != null) return token;
        switch (stack[depth - 1]) {
        case EMPTY_DOCUMENT: {
            stack[depth - 1] = NONEMPTY_DOCUMENT;
            JsonToken first = nextValue();
            if (!lenient && token != JsonToken.BEGIN_ARRAY && token != JsonToken.BEGIN_OBJECT)
                throw new IOException("Expected JSON document to start with '[' or '{' but was " + token + locationString());
            return first;
        }
        case EMPTY_ARRAY: return nextInArray(true);
        case NONEMPTY_ARRAY: return nextInArray(false);
        case EMPTY_OBJECT: return nextInObject(true);
        case DANGLING_NAME: return objectValue();
        case NONEMPTY_OBJECT: return nextInObject(false);
        case NONEMPTY_DOCUMENT: {
            int c = nextNonWhitespace(false);
            if (c == -1) return token = JsonToken.END_DOCUMENT;
            pos--;
            if (!lenient) throw syntaxError("Expected EOF");
            return nextValue();
        }
        case CLOSED: throw new IllegalStateException("JsonReader is closed");
        default: throw new AssertionError();
        }
    }
    private JsonToken advance() throws IOException {
        peek();
        JsonToken r = token;
        token = null; value = null; name = null;
        return r;
    }

    public String nextName() throws IOException {
        peek();
        if (token != JsonToken.NAME) throw new IllegalStateException("Expected a name but was " + peek());
        String r = name;
        advance();
        return r;
    }
    public String nextString() throws IOException {
        peek();
        if (token != JsonToken.STRING && token != JsonToken.NUMBER) throw new IllegalStateException("Expected a string but was " + peek());
        String r = value;
        advance();
        return r;
    }
    public boolean nextBoolean() throws IOException {
        peek();
        if (token != JsonToken.BOOLEAN) throw new IllegalStateException("Expected a boolean but was " + token);
        boolean r = "true".equals(value);
        advance();
        return r;
    }
    public void nextNull() throws IOException {
        peek();
        if (token != JsonToken.NULL) throw new IllegalStateException("Expected null but was " + token);
        advance();
    }
    public double nextDouble() throws IOException {
        peek();
        if (token != JsonToken.STRING && token != JsonToken.NUMBER) throw new IllegalStateException("Expected a double but was " + token);
        double r = Double.parseDouble(value);
        if (r >= 1.0d && value.startsWith("0")) throw new NumberFormatException("JSON forbids octal prefixes: " + value);
        if (!lenient && (Double.isNaN(r) || Double.isInfinite(r))) throw new NumberFormatException("JSON forbids NaN and infinities: " + value);
        advance();
        return r;
    }
    public long nextLong() throws IOException {
        peek();
        if (token != JsonToken.STRING && token != JsonToken.NUMBER) throw new IllegalStateException("Expected a long but was " + token);
        long r;
        try { r = Long.parseLong(value); }
        catch (NumberFormatException e) {
            double d = Double.parseDouble(value);
            r = (long) d;
            if (r != d) throw new NumberFormatException(value);
        }
        advance();
        return r;
    }
    public int nextInt() throws IOException {
        peek();
        if (token != JsonToken.STRING && token != JsonToken.NUMBER) throw new IllegalStateException("Expected an int but was " + token);
        int r;
        try { r = Integer.parseInt(value); }
        catch (NumberFormatException e) {
            double d = Double.parseDouble(value);
            r = (int) d;
            if (r != d) throw new NumberFormatException(value);
        }
        advance();
        return r;
    }
    public void close() throws IOException {
        value = null; token = null;
        stack[0] = CLOSED; depth = 1;
        in.close();
    }
    public void skipValue() throws IOException {
        int count = 0;
        do {
            JsonToken t = advance();
            if (t == JsonToken.BEGIN_ARRAY || t == JsonToken.BEGIN_OBJECT) count++;
            else if (t == JsonToken.END_ARRAY || t == JsonToken.END_OBJECT) count--;
        } while (count != 0);
    }

    private void push(int s) { if (depth == stack.length) stack = Arrays.copyOf(stack, depth * 2); stack[depth++] = s; }

    private JsonToken nextInArray(boolean first) throws IOException {
        if (first) stack[depth - 1] = NONEMPTY_ARRAY;
        else {
            switch (nextNonWhitespace(true)) {
            case ']': depth--; return token = JsonToken.END_ARRAY;
            case ';': checkLenient(); break;
            case ',': break;
            default: throw syntaxError("Unterminated array");
            }
        }
        int c = nextNonWhitespace(true);
        switch (c) {
        case ']':
            if (first) { depth--; return token = JsonToken.END_ARRAY; }
            // fall through: [1,] is [1,null] when lenient
        case ';': case ',':
            checkLenient();
            pos--;
            value = "null";
            return token = JsonToken.NULL;
        default:
            pos--;
            return nextValue();
        }
    }
    private JsonToken nextInObject(boolean first) throws IOException {
        if (first) {
            if (nextNonWhitespace(true) == '}') { depth--; return token = JsonToken.END_OBJECT; }
            pos--;
        } else {
            switch (nextNonWhitespace(true)) {
            case '}': depth--; return token = JsonToken.END_OBJECT;
            case ';': case ',': break;
            default: throw syntaxError("Unterminated object");
            }
        }
        int quote = nextNonWhitespace(true);
        switch (quote) {
        case '\'': checkLenient();   // fall through
        case '"': name = nextQuoted((char) quote); break;
        default:
            checkLenient();
            pos--;
            name = nextLiteral();
            if (name.isEmpty()) throw syntaxError("Expected name");
        }
        stack[depth - 1] = DANGLING_NAME;
        return token = JsonToken.NAME;
    }
    private JsonToken objectValue() throws IOException {
        switch (nextNonWhitespace(true)) {
        case ':': break;
        case '=':
            checkLenient();
            if ((pos < limit || fill(1)) && buf[pos] == '>') pos++;
            break;
        default: throw syntaxError("Expected ':'");
        }
        stack[depth - 1] = NONEMPTY_OBJECT;
        return nextValue();
    }
    private JsonToken nextValue() throws IOException {
        int c = nextNonWhitespace(true);
        switch (c) {
        case '{': push(EMPTY_OBJECT); return token = JsonToken.BEGIN_OBJECT;
        case '[': push(EMPTY_ARRAY); return token = JsonToken.BEGIN_ARRAY;
        case '\'': checkLenient();   // fall through
        case '"': value = nextQuoted((char) c); return token = JsonToken.STRING;
        default:
            pos--;
            value = nextLiteral();
            if (value.isEmpty()) throw syntaxError("Expected literal value");
            return token = decodeLiteral(value);
        }
    }
    private JsonToken decodeLiteral(String v) throws IOException {
        if (v.equals("null") || (lenient && v.equalsIgnoreCase("null"))) { value = "null"; return JsonToken.NULL; }
        if (v.equals("true") || (lenient && v.equalsIgnoreCase("true"))) { value = "true"; return JsonToken.BOOLEAN; }
        if (v.equals("false") || (lenient && v.equalsIgnoreCase("false"))) { value = "false"; return JsonToken.BOOLEAN; }
        char c0 = v.charAt(0);
        if (c0 == '-' || (c0 >= '0' && c0 <= '9')) {
            try { Double.parseDouble(v); return JsonToken.NUMBER; } catch (NumberFormatException e) {}
        }
        checkLenient();
        return JsonToken.STRING;
    }

    private boolean fill(int min) throws IOException {
        if (pos != limit) { limit -= pos; System.arraycopy(buf, pos, buf, 0, limit); }
        else limit = 0;
        lineStart -= pos;
        pos = 0;
        int n;
        while ((n = in.read(buf, limit, buf.length - limit)) != -1) {
            limit += n;
            if (limit >= min) return true;
        }
        return false;
    }
    private int nextNonWhitespace(boolean throwOnEof) throws IOException {
        while (pos < limit || fill(1)) {
            int c = buf[pos++];
            switch (c) {
            case '\n': line++; lineStart = pos; continue;
            case '\t': case ' ': case '\r': continue;
            case '/':
                if (pos == limit && !fill(1)) return c;
                checkLenient();
                char p = buf[pos];
                if (p == '*') { pos++; if (!skipTo("*/")) throw syntaxError("Unterminated comment"); pos += 2; continue; }
                if (p == '/') { pos++; skipToEndOfLine(); continue; }
                return c;
            case '#': checkLenient(); skipToEndOfLine(); continue;
            default: return c;
            }
        }
        if (throwOnEof) throw new EOFException("End of input" + locationString());
        return -1;
    }
    private void skipToEndOfLine() throws IOException {
        while (pos < limit || fill(1)) {
            char c = buf[pos++];
            if (c == '\n') { line++; lineStart = pos; break; }
            if (c == '\r') break;
        }
    }
    private boolean skipTo(String s) throws IOException {
        outer:
        for (; pos + s.length() <= limit || fill(s.length()); pos++) {
            if (buf[pos] == '\n') { line++; lineStart = pos + 1; }
            for (int i = 0; i < s.length(); i++) if (buf[pos + i] != s.charAt(i)) continue outer;
            return true;
        }
        return false;
    }
    private String nextQuoted(char quote) throws IOException {
        StringBuilder sb = new StringBuilder();
        while (true) {
            if (pos == limit && !fill(1)) throw syntaxError("Unterminated string");
            char c = buf[pos++];
            if (c == quote) return sb.toString();
            if (c == '\\') sb.append(readEscape());
            else { if (c == '\n') { line++; lineStart = pos; } sb.append(c); }
        }
    }
    private char readEscape() throws IOException {
        if (pos == limit && !fill(1)) throw syntaxError("Unterminated escape sequence");
        char c = buf[pos++];
        switch (c) {
        case 'u': {
            if (pos + 4 > limit && !fill(4)) throw syntaxError("Unterminated escape sequence");
            int r = 0;
            for (int i = 0; i < 4; i++) {
                char h = buf[pos + i];
                int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                if (d < 0) throw new NumberFormatException("\\u" + new String(buf, pos, 4));
                r = (r << 4) | d;
            }
            pos += 4;
            return (char) r;
        }
        case 't': return '\t';
        case 'b': return '\b';
        case 'n': return '\n';
        case 'r': return '\r';
        case 'f': return '\f';
        case '\n': line++; lineStart = pos; return c;
        default: return c;
        }
    }
    /** An unquoted value or name: up to the next delimiter. */
    private String nextLiteral() throws IOException {
        StringBuilder sb = null;
        int start = pos;
        while (true) {
            if (pos == limit) {
                if (sb == null) sb = new StringBuilder();
                sb.append(buf, start, pos - start);
                if (!fill(1)) return sb.toString();
                start = pos;
            }
            char c = buf[pos];
            switch (c) {
            case '/': case '\\': case ';': case '#': case '=': checkLenient();   // fall through
            case '{': case '}': case '[': case ']': case ':': case ',': case ' ': case '\t': case '\f': case '\r': case '\n': {
                String part = new String(buf, start, pos - start);
                return sb == null ? part : sb.append(part).toString();
            }
            default: pos++;
            }
        }
    }
    private void checkLenient() throws IOException { if (!lenient) throw syntaxError("Use JsonReader.setLenient(true) to accept malformed JSON"); }
    private IOException syntaxError(String message) throws IOException { throw new MalformedJsonException(message + locationString()); }
    private String locationString() { return " at line " + line + " column " + (pos - lineStart + 1); }
    @Override public String toString() { return getClass().getSimpleName() + locationString(); }
}
