package android.location;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class Location implements android.os.Parcelable {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final android.os.Parcelable.Creator<Location> CREATOR = new android.os.Parcelable.Creator<Location>() {
        public Location createFromParcel(android.os.Parcel p) { Location l = new Location((String)null); l.huskProps.putAll(p.readHashMap(Location.class.getClassLoader())); return l; }
        public Location[] newArray(int n) { return new Location[n]; }
    };
    public static final java.lang.String EXTRA_NO_GPS_LOCATION = "noGPSLocation";
    public static final int FORMAT_DEGREES = 0;
    public static final int FORMAT_MINUTES = 1;
    public static final int FORMAT_SECONDS = 2;
    public Location(android.location.Location p0) { set(p0); }
    public Location(java.lang.String p0) { setProvider(p0); }
    /* "[-]D[:M[:S]]" to degrees, and back (FORMAT_DEGREES 0, FORMAT_MINUTES 1, FORMAT_SECONDS 2), as AOSP formats them */
    public static double convert(java.lang.String p0) {
        if (p0 == null) throw new NullPointerException("coordinate");
        boolean neg = p0.startsWith("-"); String[] f = (neg ? p0.substring(1) : p0).split(":");
        if (f.length == 0 || f.length > 3) throw new IllegalArgumentException("coordinate=" + p0);
        double v = 0, scale = 1;
        for (String x : f) { v += Double.parseDouble(x) / scale; scale *= 60; }
        return neg ? -v : v;
    }
    public static java.lang.String convert(double p0, int p1) {
        if (p0 < -180.0 || p0 > 180.0 || Double.isNaN(p0)) throw new IllegalArgumentException("coordinate=" + p0);
        if (p1 < 0 || p1 > 2) throw new IllegalArgumentException("outputType=" + p1);
        StringBuilder sb = new StringBuilder();
        if (p0 < 0) { sb.append('-'); p0 = -p0; }
        java.text.DecimalFormat df = new java.text.DecimalFormat("###.#####");
        if (p1 == 1 || p1 == 2) {
            int deg = (int) Math.floor(p0); sb.append(deg).append(':'); p0 = (p0 - deg) * 60.0;
            if (p1 == 2) { int min = (int) Math.floor(p0); sb.append(min).append(':'); p0 = (p0 - min) * 60.0; }
        }
        sb.append(df.format(p0));
        return sb.toString();
    }
    public static void distanceBetween(double lat1, double lon1, double lat2, double lon2, float[] result) {
        if (result == null || result.length == 0) throw new IllegalArgumentException("results must not be empty");
        double p1 = Math.toRadians(lat1), p2 = Math.toRadians(lat2), d = Math.toRadians(lon2-lon1);
        double a = Math.pow(Math.sin((p2-p1)/2),2) + Math.cos(p1)*Math.cos(p2)*Math.pow(Math.sin(d/2),2);
        result[0] = (float)(6371008.8 * 2 * Math.atan2(Math.sqrt(Math.min(1,a)), Math.sqrt(Math.max(0,1-a))));
        if (result.length > 1) result[1] = (float)Math.toDegrees(Math.atan2(Math.sin(d)*Math.cos(p2), Math.cos(p1)*Math.sin(p2)-Math.sin(p1)*Math.cos(p2)*Math.cos(d)));
        if (result.length > 2) result[2] = (float)Math.toDegrees(Math.atan2(Math.sin(d)*Math.cos(p1), -Math.sin(p1)*Math.cos(p2)+Math.cos(p1)*Math.sin(p2)*Math.cos(d)));
    }
    public float bearingTo(android.location.Location p0) { float[] r = new float[2]; distanceBetween(getLatitude(),getLongitude(),p0.getLatitude(),p0.getLongitude(),r); return r[1]; }
    public int describeContents() { return 0; }
    public float distanceTo(android.location.Location p0) { float[] r = new float[1]; distanceBetween(getLatitude(),getLongitude(),p0.getLatitude(),p0.getLongitude(),r); return r[0]; }
    public void dump(android.util.Printer p0, java.lang.String p1) {}
    public float getAccuracy() { return (huskProps.get("Accuracy") instanceof Float ? (Float) huskProps.get("Accuracy") : 0f); }
    public double getAltitude() { return (huskProps.get("Altitude") instanceof Double ? (Double) huskProps.get("Altitude") : 0d); }
    public float getBearing() { return (huskProps.get("Bearing") instanceof Float ? (Float) huskProps.get("Bearing") : 0f); }
    public float getBearingAccuracyDegrees() { return (huskProps.get("BearingAccuracyDegrees") instanceof Float ? (Float) huskProps.get("BearingAccuracyDegrees") : 0f); }
    public long getElapsedRealtimeAgeMillis() { return getElapsedRealtimeAgeMillis(android.os.SystemClock.elapsedRealtime()); }
    public long getElapsedRealtimeAgeMillis(long p0) { return p0 - getElapsedRealtimeMillis(); }
    public long getElapsedRealtimeMillis() { return getElapsedRealtimeNanos()/1000000L; }
    public long getElapsedRealtimeNanos() { return (huskProps.get("ElapsedRealtimeNanos") instanceof Long ? (Long) huskProps.get("ElapsedRealtimeNanos") : 0L); }
    public double getElapsedRealtimeUncertaintyNanos() { return (huskProps.get("ElapsedRealtimeUncertaintyNanos") instanceof Double ? (Double) huskProps.get("ElapsedRealtimeUncertaintyNanos") : 0d); }
    public android.os.Bundle getExtras() { return (android.os.Bundle) huskProps.get("Extras"); }
    public double getLatitude() { return (huskProps.get("Latitude") instanceof Double ? (Double) huskProps.get("Latitude") : 0d); }
    public double getLongitude() { return (huskProps.get("Longitude") instanceof Double ? (Double) huskProps.get("Longitude") : 0d); }
    public float getMslAltitudeAccuracyMeters() { return (huskProps.get("MslAltitudeAccuracyMeters") instanceof Float ? (Float) huskProps.get("MslAltitudeAccuracyMeters") : 0f); }
    public double getMslAltitudeMeters() { return (huskProps.get("MslAltitudeMeters") instanceof Double ? (Double) huskProps.get("MslAltitudeMeters") : 0d); }
    public java.lang.String getProvider() { return (java.lang.String) huskProps.get("Provider"); }
    public float getSpeed() { return (huskProps.get("Speed") instanceof Float ? (Float) huskProps.get("Speed") : 0f); }
    public float getSpeedAccuracyMetersPerSecond() { return (huskProps.get("SpeedAccuracyMetersPerSecond") instanceof Float ? (Float) huskProps.get("SpeedAccuracyMetersPerSecond") : 0f); }
    public long getTime() { return (huskProps.get("Time") instanceof Long ? (Long) huskProps.get("Time") : 0L); }
    public float getVerticalAccuracyMeters() { return (huskProps.get("VerticalAccuracyMeters") instanceof Float ? (Float) huskProps.get("VerticalAccuracyMeters") : 0f); }
    public boolean hasAccuracy() { return huskProps.containsKey("Accuracy"); }
    public boolean hasAltitude() { return huskProps.containsKey("Altitude"); }
    public boolean hasBearing() { return huskProps.containsKey("Bearing"); }
    public boolean hasBearingAccuracy() { return huskProps.containsKey("BearingAccuracyDegrees"); }
    public boolean hasElapsedRealtimeUncertaintyNanos() { return huskProps.containsKey("ElapsedRealtimeUncertaintyNanos"); }
    public boolean hasMslAltitude() { return huskProps.containsKey("MslAltitudeMeters"); }
    public boolean hasMslAltitudeAccuracy() { return huskProps.containsKey("MslAltitudeAccuracyMeters"); }
    public boolean hasSpeed() { return huskProps.containsKey("Speed"); }
    public boolean hasSpeedAccuracy() { return huskProps.containsKey("SpeedAccuracyMetersPerSecond"); }
    public boolean hasVerticalAccuracy() { return huskProps.containsKey("VerticalAccuracyMeters"); }
    public boolean isComplete() { return getProvider() != null && hasAccuracy() && getTime() != 0 && getElapsedRealtimeNanos() != 0; }
    public boolean isFromMockProvider() { return isMock(); }
    public boolean isMock() { return (huskProps.get("Mock") instanceof Boolean ? (Boolean) huskProps.get("Mock") : false); }
    public void makeComplete() { if (getProvider() == null) setProvider("?"); if (!hasAccuracy()) setAccuracy(100); if (getTime() == 0) setTime(System.currentTimeMillis()); if (getElapsedRealtimeNanos() == 0) setElapsedRealtimeNanos(android.os.SystemClock.elapsedRealtimeNanos()); }
    public void removeAccuracy() { huskProps.remove("Accuracy"); }
    public void removeAltitude() { huskProps.remove("Altitude"); }
    public void removeBearing() { huskProps.remove("Bearing"); }
    public void removeBearingAccuracy() { huskProps.remove("BearingAccuracyDegrees"); }
    public void removeElapsedRealtimeUncertaintyNanos() { huskProps.remove("ElapsedRealtimeUncertaintyNanos"); }
    public void removeMslAltitude() { huskProps.remove("MslAltitudeMeters"); }
    public void removeMslAltitudeAccuracy() { huskProps.remove("MslAltitudeAccuracyMeters"); }
    public void removeSpeed() { huskProps.remove("Speed"); }
    public void removeSpeedAccuracy() { huskProps.remove("SpeedAccuracyMetersPerSecond"); }
    public void removeVerticalAccuracy() { huskProps.remove("VerticalAccuracyMeters"); }
    public void reset() { huskProps.clear(); }
    public void set(android.location.Location p0) { if (p0 == null) throw new NullPointerException("location"); huskProps.clear(); huskProps.putAll(p0.huskProps); if (p0.getExtras() != null) setExtras(new android.os.Bundle(p0.getExtras())); }
    public void setAccuracy(float p0) { huskProps.put("Accuracy", Float.valueOf(p0)); }
    public void setAltitude(double p0) { huskProps.put("Altitude", Double.valueOf(p0)); }
    public void setBearing(float p0) { huskProps.put("Bearing", Float.valueOf(p0)); }
    public void setBearingAccuracyDegrees(float p0) { huskProps.put("BearingAccuracyDegrees", Float.valueOf(p0)); }
    public void setElapsedRealtimeNanos(long p0) { huskProps.put("ElapsedRealtimeNanos", Long.valueOf(p0)); }
    public void setElapsedRealtimeUncertaintyNanos(double p0) { huskProps.put("ElapsedRealtimeUncertaintyNanos", Double.valueOf(p0)); }
    public void setExtras(android.os.Bundle p0) { huskProps.put("Extras", p0); }
    public void setIsFromMockProvider(boolean p0) { setMock(p0); }
    public void setLatitude(double p0) { huskProps.put("Latitude", Double.valueOf(p0)); }
    public void setLongitude(double p0) { huskProps.put("Longitude", Double.valueOf(p0)); }
    public void setMock(boolean p0) { huskProps.put("Mock", Boolean.valueOf(p0)); }
    public void setMslAltitudeAccuracyMeters(float p0) { huskProps.put("MslAltitudeAccuracyMeters", Float.valueOf(p0)); }
    public void setMslAltitudeMeters(double p0) { huskProps.put("MslAltitudeMeters", Double.valueOf(p0)); }
    public void setProvider(java.lang.String p0) { huskProps.put("Provider", p0); }
    public void setSpeed(float p0) { huskProps.put("Speed", Float.valueOf(p0)); }
    public void setSpeedAccuracyMetersPerSecond(float p0) { huskProps.put("SpeedAccuracyMetersPerSecond", Float.valueOf(p0)); }
    public void setTime(long p0) { huskProps.put("Time", Long.valueOf(p0)); }
    public void setVerticalAccuracyMeters(float p0) { huskProps.put("VerticalAccuracyMeters", Float.valueOf(p0)); }
    public void writeToParcel(android.os.Parcel p0, int p1) { p0.writeMap(huskProps); }
    Location() { this((String)null); }
}
