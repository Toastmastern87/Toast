using System;

using Toast;

namespace Sandbox
{
    public class TimeText : Entity
    {
        private UITextComponent mText;
        private float seconds;
        private float minutes;
        private float hours;
        private float sols;
        private float mars;

        void OnCreate()
        {
            mText = GetComponent<UITextComponent>();
            mText.Text = "00:00:00";
        }

        void OnUpdate(float ts)
        {
            seconds += ts;

            if (seconds >= 60.0f)
            {
                seconds -= 60.0f;
                minutes++;
            }

            if (minutes >= 60.0f)
            {
                minutes -= 60.0f;
                hours++;
            }

            if (hours >= 24.0f && minutes >= 39 && seconds >= 35.244f)
            {
                hours -= 24;
                minutes -= 39;
                seconds -= 35.244f;
                sols++;
            }

            if (sols >= 687)
            {
                sols -= 687;
                mars++;
            }

            string timeString = "";

            if (hours < 10)
                timeString = "0" + hours + ":";
            else
                timeString = hours + ":";

            if (minutes < 10)
                timeString = timeString + "0" + minutes + ":";
            else
                timeString = timeString + minutes + ":";

            if (seconds < 10)
                timeString = timeString + "0" + (int)seconds;
            else
                timeString = timeString + "" + (int)seconds;

            mText.Text = timeString;
        }

        void OnClick()
        {
        }
    }
}