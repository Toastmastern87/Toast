using System.Runtime.CompilerServices;

namespace Toast
{
    public class Console
    {
        public static void LogTrace(object message)
        {
            InternalCalls.Log_Trace(message);
        }

        public static void LogInfo(object message)
        {
            InternalCalls.Log_Info(message);
        }

        public static void LogWarning(object message)
        {
            InternalCalls.Log_Warning(message);
        }

        public static void LogError(object message)
        {
            InternalCalls.Log_Error(message);
        }

        public static void LogCritical(object message)
        {
            InternalCalls.Log_Critical(message);
        }

        public static void LogTrace(string format, params object[] args)
        {
            string message = string.Format(format, args);
            InternalCalls.Log_Trace(message);
        }

        public static void LogInfo(string format, params object[] args)
        {
            string message = string.Format(format, args);
            InternalCalls.Log_Info(message);
        }

        public static void LogWarning(string format, params object[] args)
        {
            string message = string.Format(format, args);
            InternalCalls.Log_Warning(message);
        }

        public static void LogError(string format, params object[] args)
        {
            string message = string.Format(format, args);
            InternalCalls.Log_Error(message);
        }

        public static void LogCritical(string format, params object[] args)
        {
            string message = string.Format(format, args);
            InternalCalls.Log_Critical(message);
        }
    }
}
