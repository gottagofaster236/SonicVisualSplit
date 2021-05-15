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
        private Thread taskThread;

        public CancellableLoopTask(TaskIteration taskIteration, TimeSpan iterationPeriod)
        {
            this.taskIteration = taskIteration;
            this.iterationPeriod = iterationPeriod;
        }

        public void Start()
        {
            shouldBeRunning = true;
            taskThread = new Thread(new ThreadStart(() =>
            {
                while (shouldBeRunning)
                {
                    DateTime start = DateTime.Now;
                    DateTime nextIteration = start + iterationPeriod;
                    taskIteration();
                    TimeSpan waitTime = nextIteration - DateTime.Now;
                    if (waitTime > TimeSpan.Zero)
                        Thread.Sleep(waitTime);
                }
            }));
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
