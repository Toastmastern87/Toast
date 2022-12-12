
namespace Toast
{
    public class Scene
    {
        public static bool GetRenderColliders()
        {
            return InternalCalls.Scene_GetRenderColliders();
        }

        public static void SetRenderColliders(bool value)
        {
            InternalCalls.Scene_SetRenderColliders(value);
        }

        public static float TimeScale
        {
            get => InternalCalls.Scene_GetTimeScale();
            set => InternalCalls.Scene_SetTimeScale( value);
        }
    }
}