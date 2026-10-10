package android.database;
public interface CrossProcessCursor extends Cursor { CursorWindow getWindow(); void fillWindow(int p, CursorWindow w); boolean onMove(int oldPos, int newPos); }
