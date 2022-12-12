using System.Runtime.CompilerServices;

namespace Toast
{
    public class Input
    {
        public static bool IsKeyPressed(KeyCode keyCode) 
        {
            return InternalCalls.Input_IsKeyPressed(keyCode);
        }

        public static bool IsMouseButtonPressed(MouseCode mouseCode)
        {
            return InternalCalls.Input_IsMouseButtonPressed(mouseCode);
        }

        public static Vector2 GetMousePosition()
        {
            InternalCalls.Input_GetMousePosition(out Vector2 position);
            return position;
        }

        public static float GetMouseWheelDelta() 
        {
            return InternalCalls.Input_GetMouseWheelDelta();
        }

        public static void SetMouseWheelDelta(float value)
        {
            InternalCalls.Input_SetMouseWheelDelta(value);
        }

    }
}
