/** Exercises the interpreter's constructor replacement and Java catch path. */
public final class StringAllocation {
    @SuppressWarnings("deprecation")
    public static int catchFailure(byte[] bytes) {
        try {
            String value = new String(bytes, 0);
            return value.length();
        } catch (OutOfMemoryError expected) {
            return -118;
        }
    }
}
