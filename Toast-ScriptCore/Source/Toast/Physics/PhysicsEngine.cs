using System.Linq;

namespace Toast
{
    public class PhysicsEngine
    {
        public static float GetAltitude(ulong entityID)
        {
            return InternalCalls.PhysicsEngine_GetAltitude(entityID);
        }

    }
}
