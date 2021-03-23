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
        private Task currentTask;

        public CancellableLoopTask(TaskIteration taskIteration, TimeSpan iterationPeriod)
        {
            this.taskIteration = taskIteration;
            this.iterationPeriod = iterationPeriod;
        }

        public void Start()
        {
            shouldBeRunning = true;
            currentTask = Task.Run(() =>
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
            });
        }

        public void Stop()
        {
            shouldBeRunning = false;
            currentTask.Wait();  // Wait for the thread to finish.
        }

        public delegate void TaskIteration();
    }
}
