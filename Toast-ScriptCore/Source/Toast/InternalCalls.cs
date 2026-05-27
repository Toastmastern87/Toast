using Source.Toast.Math;
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
        internal static extern bool Input_IsMouseButtonReleased(MouseCode mouseCode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Input_GetMousePosition(out Vector2 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Input_GetMouseWheelDelta();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Input_SetMouseWheelDelta(float value);

        #endregion

        #region PhysicsEngine

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float PhysicsEngine_GetAltitude(ulong entityID, bool ignoreWorldTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float PhysicsEngine_GetAltitudeAtWorldPos(Vector3 worldPos);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float PhysicsEngine_ApplyLinearImpulse(ulong entityID, Vector3 impulse);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float PhysicsEngine_ApplyLinearImpulseAtPoint(ulong entityID, Vector3 impulse, Vector3 point);

        #endregion

        #region Scene

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Scene_GetRenderTargetSize(out Vector2 outSceneSize);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Scene_GetRenderColliders();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Scene_SetRenderColliders(bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Scene_GetTimeScale();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Scene_SetTimeScale(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong Scene_AddPrefab(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong[] Scene_GetEntitiesWithPrefab(string prefabName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Scene_RequestSceneChange(string sceneName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Scene_GetWorldPosFromScreenPos(out Vector3 worldPos);

        #endregion

        #region Selection

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Selection_Clear();

        #endregion

        #region Planet

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Planet_GetTranslation(out Vector3 outTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Planet_SetTranslation(ref Vector3 inTranslation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float Planet_GetGravity();

        #endregion

        #region Script

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object Script_GetInstance(ulong entityID);

        #endregion

        #region Entity

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Entity_HasComponent(ulong entityID, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong Entity_FindEntityByName(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong Entity_FindChildEntityByName(string name, string childName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong Entity_FindParentEntity(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern ulong Entity_FindDecententByName(ulong entityID, string childName);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Entity_Select(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Entity_Deselect(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Entity_SelectExclusive(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Entity_IsSelected(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Entity_MoveTo(ulong entityID, ref Vector3 target, float speed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Entity_GetIsMoveable(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void Entity_SetIsMoveable(ulong entityID, bool value);


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
        internal static extern void TransformComponent_GetRotation(ulong entityID, out Quaternion outRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetRotation(ulong entityID, ref Quaternion inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetRotationQuaternion(ulong entityID, out Quaternion Quaternion);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetRotationQuaternion(ulong entityID, ref Quaternion inRotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetPitch(ulong entityID, out float outPitch);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetPitch(ulong entityID, ref float inPitch);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetYaw(ulong entityID, out float outYaw);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetYaw(ulong entityID, ref float inYaw);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetRoll(ulong entityID, out float outRoll);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetRoll(ulong entityID, ref float inRoll);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetScale(ulong entityID, out Vector3 outScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetScale(ulong entityID, ref Vector3 inScale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetTransform(ulong entityID, out Matrix4 outTransform);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_Rotate(ulong entityID, ref Vector3 rotationAxis, ref float angle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_RotateAroundPoint(ulong entityID, ref Vector3 point, ref Vector3 rotationAxis, ref float angle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetAngularSpeed(ulong entityID, out float result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetAngularSpeed(ulong entityID, float speed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool TransformComponent_GetIsRotating(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetIsRotating(ulong entityID, bool rotating);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_SetTargetRotation(ulong entityID, float pitchDeg, float yawDeg, float rollDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool TransformComponent_HasReachedTargetRotation(ulong entityID, float thresholdDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetWorldUp(ulong entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetWorldForward(ulong entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void TransformComponent_GetWorldRight(ulong entityID, out Vector3 result);   

        #endregion

        #region Mesh Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr MeshComponent_GetMesh(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void MeshComponent_SetMesh(ulong entityID, IntPtr unmanagedInstance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void MeshComponent_PlayAnimation(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void MeshComponent_PlayReverseAnimation(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float MeshComponent_StopAnimation(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static float MeshComponent_GetAnimationTimeElapsed(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static bool MeshComponent_IsAnimationComplete(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float MeshComponent_GetDurationAnimation(ulong entityID, string name);

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
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr CameraComponent_GetWorldTranslation(ulong entityID, out Vector3 result);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void CameraComponent_SetWorldTranslation(ulong entityID, ref Vector3 inWorldTranslation);
        //[MethodImpl(MethodImplOptions.InternalCall)]
        //internal static extern void CameraComponent_AddWorldTranslation(ulong entityID, ref Vector3 translationChange);

        #endregion

        #region UI Panel Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool UIPanelComponent_GetVisible(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UIPanelComponent_SetVisible(ulong entityID, bool value);

        #endregion

        #region UI Button Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr UIButtonComponent_GetColor(ulong entityID, out Vector4 result);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UIButtonComponent_SetColor(ulong entityID, ref Vector4 inColor);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool UIButtonComponent_GetVisible(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UIButtonComponent_SetVisible(ulong entityID, bool value);

        #endregion

        #region UI Text Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string UITextComponent_GetText(ulong entityID);
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void UITextComponent_SetText(ulong entityID, string text);

        #endregion

        #region Rigid Body Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float RigidBodyComponent_GetAltitude(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr RigidBodyComponent_GetLinearVelocity(ulong entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr RigidBodyComponent_GetAngularVelocity(ulong entityID, out Vector3 result);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RigidBodyComponent_SetMass(ulong entityID, float mass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float RigidBodyComponent_GetMass(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void RigidBodyComponent_SetAngularDamping(ulong entityID, float mass);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float RigidBodyComponent_GetAngularDamping(ulong entityID);

        #endregion

        #region Sphere Collider Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float SphereColliderComponent_GetAltitude(ulong entityID);

        #endregion

        #region Box Collider Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float BoxColliderComponent_GetAltitude(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void BoxColliderComponent_GetSize(ulong entityID, out Vector3 size);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void BoxColliderComponent_SetSize(ulong entityID, ref Vector3 size);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void BoxColliderComponent_GetOffset(ulong entityID, out Vector3 offset);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern static void BoxColliderComponent_SetOffset(ulong entityID, ref Vector3 offset);

        #endregion

        #region Particles Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool ParticlesComponent_GetEmitting(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool ParticlesComponent_SetEmitting(ulong entityID, bool value);

        #endregion

        #region Script Component

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern IntPtr ScriptComponent_GetInstance(ulong entityID);

        #endregion
    }
}
