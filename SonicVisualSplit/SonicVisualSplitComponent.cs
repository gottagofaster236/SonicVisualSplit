﻿using LiveSplit.Model;
using LiveSplit.UI.Components;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using LiveSplit.UI;
using System.Drawing;
using System.Windows.Forms;
using System.Xml;

namespace PBChance.UI.Components
{
    class PBChanceComponent : IComponent
    {
        protected InfoTextComponent InternalComponent { get; set; }
        protected SonicVisualSplitSettings Settings { get; set; }
        protected LiveSplitState State;
        protected Random rand;
        protected string category;

        string IComponent.ComponentName => "SonicVisualSplit";

        IDictionary<string, Action> IComponent.ContextMenuControls => null;
        float IComponent.HorizontalWidth => InternalComponent.HorizontalWidth;
        float IComponent.MinimumHeight => InternalComponent.MinimumHeight;
        float IComponent.MinimumWidth => InternalComponent.MinimumWidth;
        float IComponent.PaddingBottom => InternalComponent.PaddingBottom;
        float IComponent.PaddingLeft => InternalComponent.PaddingLeft;
        float IComponent.PaddingRight => InternalComponent.PaddingRight;
        float IComponent.PaddingTop => InternalComponent.PaddingTop;
        float IComponent.VerticalHeight => InternalComponent.VerticalHeight;

        XmlNode IComponent.GetSettings(XmlDocument document)
        {
            return Settings.GetSettings(document);
        }

        Control IComponent.GetSettingsControl(LayoutMode mode)
        {
            Settings.Mode = mode;
            return Settings;
        }

        void IComponent.SetSettings(XmlNode settings)
        {
            Settings.SetSettings(settings);
        }

        public PBChanceComponent(LiveSplitState state)
        {
            State = state;
            InternalComponent = new InfoTextComponent("PB Chance                   .", "0.0%")
            {
                AlternateNameText = new string[]
                {
                    "PB Chance",
                    "PB%:"
                }
            };
            Settings = new SonicVisualSplitSettings();
            Settings.SettingChanged += OnSettingChanged;
            rand = new Random();
            category = State.Run.GameName + State.Run.CategoryName;

            state.OnSplit += OnSplit;
            state.OnReset += OnReset;
            state.OnSkipSplit += OnSkipSplit;
            state.OnUndoSplit += OnUndoSplit;
            state.OnStart += OnStart;
            state.RunManuallyModified += OnRunManuallyModified;

            Recalculate();
        }

        private void OnRunManuallyModified(object sender, EventArgs e)
        {
            Recalculate();
        }

        private void OnSettingChanged(object sender, EventArgs e)
        {
            Recalculate();
        }

        private void OnStart(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnUndoSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnSkipSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void OnReset(object sender, TimerPhase value)
        {
            Recalculate();
        }

        protected void OnSplit(object sender, EventArgs e)
        {
            Recalculate();
        }

        protected void Recalculate()
        {
            // Get the current Personal Best, if it exists
            Time pb = State.Run.Last().PersonalBestSplitTime;

            if (pb[State.CurrentTimingMethod] == TimeSpan.Zero)
            {
                // No personal best, so any run will PB
                InternalComponent.InformationValue = "100%";
                return;
            }

            // Create the lists of split times
            List<Time?>[] splits = new List<Time?>[State.Run.Count];
            for (int i = 0; i < State.Run.Count; i++)
            {
                splits[i] = new List<Time?>();
            }

            // Find the range of attempts to gather times from
            int lastAttempt = State.Run.AttemptHistory.Count;
            int runCount = State.Run.AttemptHistory.Count;
            if (!Settings.IgnoreRunCount)
            {
                runCount = State.Run.AttemptCount;
                if (runCount > State.Run.AttemptHistory.Count)
                {
                    runCount = State.Run.AttemptHistory.Count;
                }
            }
            int firstAttempt = lastAttempt / 2;
            if (Settings.UseFixedAttempts)
            {
                // Fixed number of attempts
                firstAttempt = lastAttempt - Settings.AttemptCount;

                if (firstAttempt < State.Run.GetMinSegmentHistoryIndex())
                {
                    firstAttempt = State.Run.GetMinSegmentHistoryIndex();
                }
            }
            else
            {
                // Percentage of attempts
                firstAttempt = lastAttempt - runCount * Settings.AttemptCount / 100;
                if (firstAttempt < State.Run.GetMinSegmentHistoryIndex())
                {
                    firstAttempt = State.Run.GetMinSegmentHistoryIndex();
                }
            }

            // Gather split times
            for (int a = firstAttempt; a < lastAttempt; a++)
            {
                int lastSegment = -1;

                // Get split times from a single attempt
                for (int segment = 0; segment < State.Run.Count; segment++)
                {
                    if (State.Run[segment].SegmentHistory == null || State.Run[segment].SegmentHistory.Count == 0)
                    {
                        InternalComponent.InformationValue = "-";
                        return;
                    }

                    if (State.Run[segment].SegmentHistory.ContainsKey(a) && State.Run[segment].SegmentHistory[a][State.CurrentTimingMethod] > TimeSpan.Zero)
                    {
                        splits[segment].Add(State.Run[segment].SegmentHistory[a]);
                        lastSegment = segment;
                    }
                }

                if (lastSegment < State.Run.Count - 1)
                {
                    // Run didn't finish, add "reset" for the last known split
                    splits[lastSegment + 1].Add(null);
                }
            }

            // Calculate probability of PB
            int success = 0;
            for (int i = 0; i < 10000; i++)
            {
                // Get current time as a baseline
                Time test = State.CurrentTime;
                if (test[State.CurrentTimingMethod] < TimeSpan.Zero)
                {
                    test[State.CurrentTimingMethod] = TimeSpan.Zero;
                }

                // Add random split times for each remaining segment
                for (int segment = 0; segment < State.Run.Count; segment++)
                {
                    if (segment < State.CurrentSplitIndex)
                    {
                        continue;
                    }

                    if (splits[segment].Count == 0)
                    {
                        // This split contains no split times, so we cannot calculate a probability
                        InternalComponent.InformationValue = "-";
                        return;
                    }

                    int attempt = rand.Next(splits[segment].Count);
                    Time? split = splits[segment][attempt];
                    if (split == null)
                    {
                        // Split is a reset, so count it as a failure
                        test += pb;
                        break;
                    }
                    else
                    {
                        // Add the split time
                        test += split.Value;
                    }
                }

                if (test[State.CurrentTimingMethod] < pb[State.CurrentTimingMethod])
                {
                    success++;
                }
            }

            double prob = success / 10000.0;
            string text = (prob * 100.0).ToString() + "%";
            if (Settings.DisplayOdds && prob > 0)
            {
                text += " (1 in " + Math.Round(1 / prob, 2).ToString() + ")";
            }
            InternalComponent.InformationValue = text;
        }

        void IComponent.DrawHorizontal(Graphics g, LiveSplitState state, float height, Region clipRegion)
        {
            PrepareDraw(state, LayoutMode.Horizontal);
            InternalComponent.DrawHorizontal(g, state, height, clipRegion);
        }

        void IComponent.DrawVertical(Graphics g, LiveSplitState state, float width, Region clipRegion)
        {
            InternalComponent.PrepareDraw(state, LayoutMode.Vertical);
            PrepareDraw(state, LayoutMode.Vertical);
            InternalComponent.DrawVertical(g, state, width, clipRegion);
        }

        void PrepareDraw(LiveSplitState state, LayoutMode mode)
        {
            InternalComponent.NameLabel.ForeColor = state.LayoutSettings.TextColor;
            InternalComponent.ValueLabel.ForeColor = state.LayoutSettings.TextColor;
            InternalComponent.PrepareDraw(state, mode);
        }

        void IComponent.Update(IInvalidator invalidator, LiveSplitState state, float width, float height, LayoutMode mode)
        {
            string newCategory = State.Run.GameName + State.Run.CategoryName;
            if (newCategory != category)
            {
                Recalculate();
                category = newCategory;
            }

            InternalComponent.Update(invalidator, state, width, height, mode);
        }

        void IDisposable.Dispose()
        {

        }
    }
}