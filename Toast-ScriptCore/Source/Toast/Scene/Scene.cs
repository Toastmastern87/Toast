using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Toast
{
    public class Scene
    {
        public static bool GetRenderColliders()
        {
            return GetRenderColliders_Native();
        }

        public static void SetRenderColliders(bool value)
        {
            SetRenderColliders_Native(value);
        }

        public static float TimeScale
        {
            get => GetTimeScale_Native();
            set => SetTimeScale_Native( value);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetRenderColliders_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool SetRenderColliders_Native(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float GetTimeScale_Native();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetTimeScale_Native(float value);
    }
}