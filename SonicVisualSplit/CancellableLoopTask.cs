using System;
using System.Threading;
using System.Threading.Tasks;

namespace SonicVisualSplit
{
    class CancellableLoopTask
    {
        private TaskIteration taskIteration;
        private TimeSpan iterationPeriod;
        private volatile bool shouldBeRunning = false;
        private object iterationRunningLock = new object();

        public CancellableLoopTask(TaskIteration taskIteration, TimeSpan iterationPeriod)
        {
            this.taskIteration = taskIteration;
            this.iterationPeriod = iterationPeriod;
        }

        public void Start()
        {
            shouldBeRunning = true;
            Task.Run(() =>
            {
                while (shouldBeRunning)
                {
                    lock (iterationRunningLock)
                    {
                        DateTime start = DateTime.Now;
                        DateTime nextIteration = start + iterationPeriod;
                        taskIteration();
                        TimeSpan waitTime = nextIteration - DateTime.Now;
                        if (waitTime > TimeSpan.Zero)
                            Thread.Sleep(waitTime);
                    }
                }
            });
        }

        public void Stop()
        {
            shouldBeRunning = false;
            lock (iterationRunningLock) { }  // Make sure that the thread stops.
        }

        public delegate void TaskIteration();
    }
}
