using System;

namespace Toast 
{

    public class Main
    {
        public float FloatVar { get; set; }

        public Main()
        {
            System.Console.WriteLine("Main constructor!");
        }

        public void PrintMessage()
        {
            System.Console.WriteLine("Hello World from C#!");
        }

        public void PrintInt(int number)
        {
            System.Console.WriteLine($"C# says: {number}");
        }

        public void PrintCustomMessage(string message)
        {
            System.Console.WriteLine($"C# says: {message}");
        }
    }

}
