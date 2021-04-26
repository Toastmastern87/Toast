using System.Runtime.CompilerServices;

namespace Toast
{
    public class Input
    {
        public static bool IsKeyPressed(KeyCode keyCode) 
        {
            return IsKeyPressed_Native(keyCode);
        }

        public static bool IsMouseButtonPressed(MouseCode mouseCode)
        {
            return IsMouseButtonPressed_Native(mouseCode);
        }

        public static float GetMouseWheelDelta() 
        {
            return GetMouseWheelDelta_Native();
        }

        public static void SetMouseWheelDelta(float value)
        {
            SetMouseWheelDelta_Native(value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsKeyPressed_Native(KeyCode keyCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool IsMouseButtonPressed_Native(MouseCode mouseCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetMouseWheelDelta_Native();


        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetMouseWheelDelta_Native(float value);
    }
}
