using System;
using System.Diagnostics;
using System.Threading;

namespace SonicVisualSplit
{
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
            taskThread?.Join();  // Wait for the thread to finish.
        }

        public delegate void TaskIteration();
    }
}
