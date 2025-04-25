using System.Linq;

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

        public static Entity AddPrefab(string name)
        {
            ulong entityID = InternalCalls.Scene_AddPrefab(name);

            if (entityID == 0)
                return null;

            return new Entity(entityID);
        }

        public static Entity[] GetPrefabEntities(string prefabName)
        {
            ulong[] entityIDs = InternalCalls.Scene_GetEntitiesWithPrefab(prefabName);

            Entity[] returnEntities = new Entity[entityIDs.Length];

            for (int i = 0; i < entityIDs.Length; i++)
                returnEntities[i] = new Entity(entityIDs[i]);

            return returnEntities;
        }
    }
}