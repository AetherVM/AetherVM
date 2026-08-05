# AetherDbg
The built-in lldb-server of AetherVM for gdb/lldb/Cutter.

## Architecture
```
+------------------------------------------------------------+
|                GDB / LLDB / Cutter Client                  |
+------------------------------------------------------------+
                             |
                   GDB RSP (TCP / Socket)
                             v
+------------------------------------------------------------+
|             GDBRemoteCommunicationServerLLGS               |
|      (Unchanged LLDB class handling RSP framing & logic)   |
+------------------------------------------------------------+
                             |
               C++ Virtual Method Calls
            (GetRegisterContext, ReadMemory, etc.)
                             v
+----------------------------+-------------------------------+
|   VNativeProcessProtocol   |     VNativeThreadProtocol     |
|     (AetherVM Process)     |       (AetherVM Thread)       |
+----------------------------+-------------------------------+
                             |
                     Direct Memory Access
                             v
+------------------------------------------------------------+
|                        CPUState                            |
|                 (AetherVM Virtual CPU)                     |
+------------------------------------------------------------+
```
