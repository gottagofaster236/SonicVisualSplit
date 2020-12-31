using SonicVisualSplitWrapper;
using System.Diagnostics;
using System.Drawing;

namespace SonicVisualSplit
{
    class DigitsRecognizer
    {
        // https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv/
        public static (AnalysisResult, long) Test()
        {
            string templatesDirectory = "C:\\Users\\Пользователь\\Desktop\\ps\\Sonic 1@Composite";
            Stopwatch stopWatch = Stopwatch.StartNew();
            var result = BaseWrapper.AnalyzeFrame("Sonic 1", templatesDirectory, false, true);
            stopWatch.Stop();
            return (result, stopWatch.ElapsedMilliseconds);
        }
    }
}
