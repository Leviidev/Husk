package android.util;

/** Thrown when a reader meets JSON it cannot read. */
public final class MalformedJsonException extends java.io.IOException {
    public MalformedJsonException(String message) { super(message); }
}
