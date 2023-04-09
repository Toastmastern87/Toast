using System;

namespace Toast
{
    public class Animation
    {
        public Animation()
        {

        }

        internal Animation(IntPtr unmanagedInstance)
        {
            mUnmanagedInstance = unmanagedInstance;
        }

        internal IntPtr mUnmanagedInstance;
    }
}
