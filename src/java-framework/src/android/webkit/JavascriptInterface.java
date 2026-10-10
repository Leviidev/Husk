package android.webkit;

import java.lang.annotation.*;

/** Marks the methods of an addJavascriptInterface object the page may call. */
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.METHOD})
public @interface JavascriptInterface {}
