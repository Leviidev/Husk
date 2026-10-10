package android.app.job;
public abstract class JobScheduler {
    public static final int RESULT_FAILURE = 0, RESULT_SUCCESS = 1;
    public abstract int schedule(JobInfo job); public abstract int enqueue(JobInfo job, JobWorkItem w); public abstract void cancel(int id); public abstract void cancelAll();
    public abstract java.util.List<JobInfo> getAllPendingJobs(); public abstract JobInfo getPendingJob(int id);
    /** Jobs are kept but not run: Husk runs one app in the foreground. */
    public static final class Impl extends JobScheduler {
        private final java.util.HashMap<Integer, JobInfo> mJobs = new java.util.HashMap<>();
        public int schedule(JobInfo j) { mJobs.put(j.getId(), j); return RESULT_SUCCESS; }
        public int enqueue(JobInfo j, JobWorkItem w) { return schedule(j); }
        public void cancel(int id) { mJobs.remove(id); } public void cancelAll() { mJobs.clear(); }
        public java.util.List<JobInfo> getAllPendingJobs() { return new java.util.ArrayList<>(mJobs.values()); }
        public JobInfo getPendingJob(int id) { return mJobs.get(id); }
    }
}
