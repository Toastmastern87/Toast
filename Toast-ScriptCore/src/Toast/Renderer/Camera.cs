using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace Toast
{
    public class Camera
    {
        public Camera()
        {

        }

        internal Camera(IntPtr unmanagedInstance)
        {
            mUnmanagedInstance = unmanagedInstance;
        }

        internal IntPtr mUnmanagedInstance;
    }
}
