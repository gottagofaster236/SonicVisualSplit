using SonicVisualSplitWrapper;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;

namespace SonicVisualSplit
{
    class DigitsRecognizer
    {
        // https://www.pyimagesearch.com/2015/01/26/multi-scale-template-matching-using-python-opencv/
        public static (AnalysisResult, long) Test()
        {
            string templatesDirectory = Path.GetFullPath("../../../../Templates/Sonic 1@Composite");
            Stopwatch stopWatch = Stopwatch.StartNew();
            var result = BaseWrapper.AnalyzeFrame("Sonic 1", templatesDirectory, false, false, true, true);
            stopWatch.Stop();
            return (result, stopWatch.ElapsedMilliseconds);
        }
    }
}
