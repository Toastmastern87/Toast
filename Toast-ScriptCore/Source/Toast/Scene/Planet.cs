using System.Linq;


namespace Toast
{
    public static class Planet
    {
        public static Vector3 Translation
        {
            get
            {
                InternalCalls.Planet_GetTranslation(out Vector3 result);
                return result;
            }

            set
            {
                InternalCalls.Planet_SetTranslation(ref value);
            }
        }
    }
}
