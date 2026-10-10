package android.webkit;

import java.io.InputStream;
import java.util.Map;

/** What shouldInterceptRequest answers with: the bytes of a resource the app serves itself. */
public class WebResourceResponse {
    private String mMimeType, mEncoding, mReasonPhrase;
    private int mStatusCode = 200;
    private Map<String, String> mResponseHeaders;
    private InputStream mInputStream;
    public WebResourceResponse(String mimeType, String encoding, InputStream data) { mMimeType = mimeType; mEncoding = encoding; mInputStream = data; }
    public WebResourceResponse(String mimeType, String encoding, int statusCode, String reasonPhrase, Map<String, String> responseHeaders, InputStream data) {
        this(mimeType, encoding, data);
        setStatusCodeAndReasonPhrase(statusCode, reasonPhrase);
        mResponseHeaders = responseHeaders;
    }
    public WebResourceResponse(boolean immutable, String mimeType, String encoding, int statusCode, String reasonPhrase, Map<String, String> responseHeaders, InputStream data) {
        this(mimeType, encoding, statusCode, reasonPhrase, responseHeaders, data);
    }
    public void setMimeType(String mimeType) { mMimeType = mimeType; }
    public String getMimeType() { return mMimeType; }
    public void setEncoding(String encoding) { mEncoding = encoding; }
    public String getEncoding() { return mEncoding; }
    public void setStatusCodeAndReasonPhrase(int statusCode, String reasonPhrase) {
        if (statusCode < 100 || statusCode > 599) throw new IllegalArgumentException("statusCode can't be less than 100 or greater than 599.");
        if (reasonPhrase == null || reasonPhrase.trim().isEmpty()) throw new IllegalArgumentException("reasonPhrase can't be empty.");
        mStatusCode = statusCode; mReasonPhrase = reasonPhrase;
    }
    public int getStatusCode() { return mStatusCode; }
    public String getReasonPhrase() { return mReasonPhrase; }
    public void setResponseHeaders(Map<String, String> headers) { mResponseHeaders = headers; }
    public Map<String, String> getResponseHeaders() { return mResponseHeaders; }
    public void setData(InputStream data) { mInputStream = data; }
    public InputStream getData() { return mInputStream; }
}
