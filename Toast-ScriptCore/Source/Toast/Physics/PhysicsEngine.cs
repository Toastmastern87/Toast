using System.Linq;

namespace Toast
{
    public class PhysicsEngine
    {
        public static float GetAltitude(ulong entityID, bool ignoreWorldTranslation)
        {
            return InternalCalls.PhysicsEngine_GetAltitude(entityID, ignoreWorldTranslation);
        }

        public static float GetAltitudeAtWorldPos(Vector3 worldPos)
        {
            return InternalCalls.PhysicsEngine_GetAltitudeAtWorldPos(worldPos);
        }

    }
}
