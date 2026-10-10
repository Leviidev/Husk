package android.util;

import java.io.Closeable;
import java.io.IOException;
import java.io.Writer;
import java.util.Arrays;

/** Writes a JSON document a token at a time. */
public final class JsonWriter implements Closeable {
    private static final int EMPTY_ARRAY = 1, NONEMPTY_ARRAY = 2, EMPTY_OBJECT = 3, DANGLING_NAME = 4, NONEMPTY_OBJECT = 5,
            EMPTY_DOCUMENT = 6, NONEMPTY_DOCUMENT = 7;
    private final Writer out;
    private int[] stack = new int[32];
    private int depth;
    private String indent, separator = ":";
    private boolean lenient;

    public JsonWriter(Writer out) {
        if (out == null) throw new NullPointerException("out == null");
        this.out = out;
        stack[depth++] = EMPTY_DOCUMENT;
    }
    public void setIndent(String indent) {
        if (indent == null || indent.isEmpty()) { this.indent = null; separator = ":"; }
        else { this.indent = indent; separator = ": "; }
    }
    public void setLenient(boolean lenient) { this.lenient = lenient; }
    public boolean isLenient() { return lenient; }

    public JsonWriter beginArray() throws IOException { return open(EMPTY_ARRAY, "["); }
    public JsonWriter endArray() throws IOException { return close(EMPTY_ARRAY, NONEMPTY_ARRAY, "]"); }
    public JsonWriter beginObject() throws IOException { return open(EMPTY_OBJECT, "{"); }
    public JsonWriter endObject() throws IOException { return close(EMPTY_OBJECT, NONEMPTY_OBJECT, "}"); }
    private JsonWriter open(int empty, String bracket) throws IOException {
        beforeValue(true);
        if (depth == stack.length) stack = Arrays.copyOf(stack, depth * 2);
        stack[depth++] = empty;
        out.write(bracket);
        return this;
    }
    private JsonWriter close(int empty, int nonempty, String bracket) throws IOException {
        int ctx = stack[depth - 1];
        if (ctx != nonempty && ctx != empty) throw new IllegalStateException("Nesting problem");
        depth--;
        if (ctx == nonempty) newline();
        out.write(bracket);
        return this;
    }
    public JsonWriter name(String name) throws IOException {
        if (name == null) throw new NullPointerException("name == null");
        int ctx = stack[depth - 1];
        if (ctx == NONEMPTY_OBJECT) out.write(',');
        else if (ctx != EMPTY_OBJECT) throw new IllegalStateException("Nesting problem");
        newline();
        stack[depth - 1] = DANGLING_NAME;
        string(name);
        return this;
    }
    public JsonWriter value(String value) throws IOException {
        if (value == null) return nullValue();
        beforeValue(false);
        string(value);
        return this;
    }
    public JsonWriter nullValue() throws IOException { beforeValue(false); out.write("null"); return this; }
    public JsonWriter value(boolean value) throws IOException { beforeValue(false); out.write(value ? "true" : "false"); return this; }
    public JsonWriter value(double value) throws IOException {
        if (!lenient && (Double.isNaN(value) || Double.isInfinite(value))) throw new IllegalArgumentException("Numeric values must be finite, but was " + value);
        beforeValue(false);
        out.append(Double.toString(value));
        return this;
    }
    public JsonWriter value(long value) throws IOException { beforeValue(false); out.write(Long.toString(value)); return this; }
    public JsonWriter value(Number value) throws IOException {
        if (value == null) return nullValue();
        String s = value.toString();
        if (!lenient && (s.equals("-Infinity") || s.equals("Infinity") || s.equals("NaN"))) throw new IllegalArgumentException("Numeric values must be finite, but was " + value);
        beforeValue(false);
        out.append(s);
        return this;
    }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException {
        out.close();
        if (depth > 1 || (depth == 1 && stack[0] != NONEMPTY_DOCUMENT)) throw new IOException("Incomplete document");
        depth = 0;
    }

    private void string(String v) throws IOException {
        out.write('"');
        for (int i = 0, n = v.length(); i < n; i++) {
            char c = v.charAt(i);
            switch (c) {
            case '"': case '\\': out.write('\\'); out.write(c); break;
            case '\t': out.write("\\t"); break;
            case '\b': out.write("\\b"); break;
            case '\n': out.write("\\n"); break;
            case '\r': out.write("\\r"); break;
            case '\f': out.write("\\f"); break;
            case ' ': case ' ': out.write(String.format("\\u%04x", (int) c)); break;
            default:
                if (c <= 0x1F) out.write(String.format("\\u%04x", (int) c));
                else out.write(c);
            }
        }
        out.write('"');
    }
    private void newline() throws IOException {
        if (indent == null) return;
        out.write("\n");
        for (int i = 1; i < depth; i++) out.write(indent);
    }
    private void beforeValue(boolean root) throws IOException {
        switch (stack[depth - 1]) {
        case EMPTY_DOCUMENT:
            if (!lenient && !root) throw new IllegalStateException("JSON must start with an array or an object.");
            stack[depth - 1] = NONEMPTY_DOCUMENT;
            break;
        case EMPTY_ARRAY: stack[depth - 1] = NONEMPTY_ARRAY; newline(); break;
        case NONEMPTY_ARRAY: out.append(','); newline(); break;
        case DANGLING_NAME: out.append(separator); stack[depth - 1] = NONEMPTY_OBJECT; break;
        case NONEMPTY_DOCUMENT: throw new IllegalStateException("JSON must have only one top-level value.");
        default: throw new IllegalStateException("Nesting problem");
        }
    }
}
