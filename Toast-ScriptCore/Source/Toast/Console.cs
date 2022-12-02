using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

namespace Toast
{
    public class Console
    {
        public static void LogTrace(object message)
        {
            LogTrace_Native(message);
        }

        public static void LogInfo(object message)
        {
            LogInfo_Native(message);
        }

        public static void LogWarning(object message)
        {
            LogWarning_Native(message);
        }

        public static void LogError(object message)
        {
            LogError_Native(message);
        }

        public static void LogCritical(object message)
        {
            LogCritical_Native(message);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogTrace_Native(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogInfo_Native(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogWarning_Native(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogError_Native(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogCritical_Native(object message);
    }
}
