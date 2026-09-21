using System.Runtime.InteropServices;

namespace Toast {

    public enum EventType : uint
    {
        None = 0,
        WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
        AppTick, AppUpdate, AppRender,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Event
    {
        public EventType Type;

        public uint Key;
        public uint RepeatCount;
        public uint MouseButton;

        public float MouseX;
        public float MouseY;
        public float ScrollDelta;

        public bool IsKeyPressed(KeyCode key) 
            => Type == EventType.KeyPressed && (KeyCode)Key == key;

        public bool IsKeyReleased(KeyCode key)
            => Type == EventType.KeyReleased && (KeyCode)Key == key;

        public bool IsMouseButtonPressed(MouseCode button)
            => Type == EventType.MouseButtonPressed && (MouseCode)MouseButton == button;
    }

}
