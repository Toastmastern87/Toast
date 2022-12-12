using System;
using System.Runtime.CompilerServices;

namespace Toast
{
    public static class InternalCalls
    {
        #region Log

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log_Trace(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log_Info(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log_Warning(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log_Error(object message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Log_Critical(object message);

        #endregion

        #region Input

        [MethodImplAttribute(MethodImplOptions.InternalCall)]
        internal extern static bool Input_IsKeyPressed(KeyCode keycode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Input_IsMouseButtonPressed(MouseCode mouseCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Input_GetMousePosition(out Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Input_GetMouseWheelDelta();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Input_SetMouseWheelDelta(float value);

        #endregion

        #region Scene

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Scene_GetRenderColliders();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Scene_SetRenderColliders(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Scene_GetTimeScale();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Scene_SetTimeScale(float value);

        #endregion

        #region Entity

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Entity_HasComponent(ulong entityID, Type componentType);

        #endregion

        #region Tag Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string TagComponent_GetTag(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TagComponent_SetTag(ulong entityID, string tag);

        #endregion

        #region Transform Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetTranslation(ulong entityID, out Vector3 outTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetTranslation(ulong entityID, ref Vector3 inTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetRotation(ulong entityID, out Vector3 outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetRotation(ulong entityID, ref Vector3 inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetScale(ulong entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetScale(ulong entityID, ref Vector3 inScale);

        #endregion

        #region Mesh Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr MeshComponent_GetMesh(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void MeshComponent_SetMesh(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void MeshComponent_RegeneratePlanet(ulong entityID, Vector3 cameraPos, Matrix4 cameraForward);

        #endregion

        #region Camera Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr CameraComponent_GetCamera(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void CameraComponent_SetCamera(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void CameraComponent_SetFarClip(ulong entityID, float farClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float CameraComponent_GetFarClip(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void CameraComponent_SetNearClip(ulong entityID, float nearClip);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float CameraComponent_GetNearClip(ulong entityID);

        #endregion

        #region Planet Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr PlanetComponent_GetMesh(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PlanetComponent_SetMesh(ulong entityID, IntPtr unmanagedInstance);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PlanetComponent_RegeneratePlanet(ulong entityID, Vector3 cameraPos, Matrix4 cameraForward);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PlanetComponent_GetRadius(ulong entityID, out float inScale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void PlanetComponent_GetSubdivisions(ulong entityID, out int inScale);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern double[] PlanetComponent_GetDistanceLUT(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern double[] PlanetComponent_GetFaceLevelDotLUT(ulong entityID);

        #endregion

        #region UIButton Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr UIButtonComponent_GetColor(ulong entityID, out Vector4 result);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UIButtonComponent_SetColor(ulong entityID, ref Vector4 inColor);

        #endregion

        #region UIText Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string UITextComponent_GetText(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UITextComponent_SetText(ulong entityID, string text);

        #endregion
    }
}
