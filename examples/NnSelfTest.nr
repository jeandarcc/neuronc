module NN;

Init method() 
{
    Print("NN self-test running...");
    testResult is NN.SelfTest();
    Print(testResult);

    if (testResult == 1) {
        Print("NN self-test passed.");
    } else {
        Print("NN self-test failed.");
    }

    
}
