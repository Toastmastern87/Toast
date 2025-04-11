using System;
using System.IO;
using System.Threading;

using System.Globalization;
using Toast;

namespace Sandbox
{
    public class Money : Entity
    {
        public int mCurrentMoney;

        private int mTargetMoney;
        private int mStartMoney;
        private float mCountdownDuration;  
        private float mCountdownElapsed;
        private bool mIsCountingDown;

        private UITextComponent mMoneyText;
        private NumberFormatInfo mNFI;

        void OnCreate()
        {
            mMoneyText = GetComponent<UITextComponent>();

            mCurrentMoney = 400000;
            mTargetMoney = mCurrentMoney;

            mNFI = new NumberFormatInfo()
            {
                NumberGroupSeparator = ".",  // Set the thousands separator to '.'
                NumberDecimalDigits = 0,       // No decimal digits
                NumberGroupSizes = new int[] { 3 }
            };

            mIsCountingDown = false;
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
            if (mIsCountingDown)
            {
                mCountdownElapsed += ts;
                float t = mCountdownElapsed / mCountdownDuration;
                if (t >= 1f)
                {
                    t = 1f;
                    mIsCountingDown = false;
                }

                double interpolatedValue = mStartMoney + (mTargetMoney - mStartMoney) * t;
                mCurrentMoney = (int)Math.Round(interpolatedValue);
            }


            mMoneyText.Text = "$" + mCurrentMoney.ToString("N", mNFI);
        }

        public void SetRetracttMoney(int retractMoney)
        {
            mStartMoney = mCurrentMoney;
            mTargetMoney -= retractMoney;

            // Calculate the absolute difference.
            int diff = Math.Abs(mCurrentMoney - mTargetMoney);
            // Determine a duration for the animation.
            // For example: 1 second per 100,000 units difference, with a minimum of 0.5 seconds.
            mCountdownDuration = Math.Max(0.5f, diff / 100000.0f);

            mCountdownElapsed = 0f;
            mIsCountingDown = true;
        }
    }
}
