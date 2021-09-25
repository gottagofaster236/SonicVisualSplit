using System;
using System.Diagnostics;
using System.Threading;

namespace SonicVisualSplit
{
    // Represents a task that's executed repeatedly. This class isn't thread-safe.
    class CancellableLoopTask
    {
        private TaskIteration taskIteration;
        private TimeSpan iterationPeriod;
        private volatile bool shouldBeRunning = false;
        private Thread taskThread;

        public CancellableLoopTask(TaskIteration taskIteration, TimeSpan iterationPeriod)
        {
            this.taskIteration = taskIteration;
            this.iterationPeriod = iterationPeriod;
        }

        public void Start()
        {
            if (shouldBeRunning)
            {
                return;
            }
            shouldBeRunning = true;
            taskThread = new Thread(() =>
            {
                while (shouldBeRunning)
                {
                    var stopwatch = new Stopwatch();
                    stopwatch.Start();
                    taskIteration();
                    var waitTime = iterationPeriod - stopwatch.Elapsed;
                    if (waitTime > TimeSpan.Zero)
                    {
                        Thread.Sleep(waitTime);
                    }
                }
            });
            taskThread.Start();
        }

        public void Stop()
        {
            shouldBeRunning = false;
            taskThread?.Join();
            taskThread = null;
        }

        public delegate void TaskIteration();
    }
}
