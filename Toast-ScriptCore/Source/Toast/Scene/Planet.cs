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

        public static double Gravity
        {
            get
            {
                return InternalCalls.Planet_GetGravity();
            }

            set
            {
                ;// TO DO
            }
        }
    }
}
