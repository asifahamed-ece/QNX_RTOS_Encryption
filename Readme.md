# 🛡️ QNX Concurrent File Encryption Service

> **Capstone Project** | QNX Neutrino RTOS | **Category**: INTERMEDIATE  
> **Author**: Asifa Hamed | **Course**: QNX RTOS Training Program  
> **Environment**: VMware Workstation Pro + QNX SDP 8.0 + Momentics IDE  

---

## 📋 Table of Contents
- [Prerequisites Compliance](#-prerequisites-compliance)
- [Project Description](#-project-description)
- [Build Instructions](#-build-instructions)
- [Execution Guide](#-execution-guide)
---

## ✅ Prerequisites Compliance

This project strictly implements **all 10 mandatory requirements** from the QNX Capstone guideline:

| # | Requirement | Implementation | Verification |
|---|-------------|----------------|--------------|
| **1** | Must use QNX Message API | `MsgSendv()`, `MsgReceivev()`, `MsgReplyv()` | ✅ Implemented |
| **2** | Client-Server Architecture | Separate `qnx_encrypt_client` & `qnx_encrypt_server` executables | ✅ Implemented |
| **3** | At least one IOV transfer | `iov_t` structures for zero-copy 64KB+ payload | ✅ Implemented |
| **4** | Payload ≥ 64KB | `ENCRYPTION_PAYLOAD_SIZE = 66,584 bytes` | ✅ Verified |
| **5** | Common header file | `messages.h` shared identically across both projects | ✅ Implemented |
| **6** | Well-commented code | Doxygen-style comments, meaningful variable names | ✅ Implemented |
| **7** | 10-second client timeout | `sigaction(SIGALRM)` + retry loop | ✅ Implemented |
| **8** | Check all API returns | Every system call validated with `if(ret == -1)` | ✅ Implemented |
| **9** | Client uses `name_open` ONLY | `name_open(ENCRYPTION_SERVICE_NAME, O_RDWR)` | ✅ Implemented |
| **10** | Server async response | `MsgSendPulse()` with `SIGEV_PULSE_INIT` | ✅ Implemented |

---

## 📖 Project Description

**Category**: INTERMEDIATE - RTOS Concepts + Programming

The **Concurrent File Encryption Service** demonstrates advanced QNX Neutrino RTOS capabilities through a production-grade client-server application:

### Core Features:
- **Large Data Transfer**: Client sends **66KB+ data blocks** to server
- **Concurrent Processing**: Server creates a **thread pool** (2 worker threads) to encrypt different segments simultaneously
- **Thread Synchronization**: 
  - **Mutexes** protect shared resources (completion counter)
  - **Condition Variables** signal when all threads complete
- **Encryption Algorithm**: XOR cipher (demonstration - easily replaceable with AES/DES)
- **Asynchronous Response**: Server sends encrypted data back via message + pulse notification
- **Robust Error Handling**: Timeouts, retry logic, and comprehensive error checking

---


---

## 🛠️ Build Instructions

### Prerequisites

Before building, ensure you have:

1. **QNX Software Development Platform (SDP) 8.0**
   - Download from: [BlackBerry QNX SDP](https://blackberry.qnx.com/en)
   - Install Momentics IDE for QNX (recommended) or use command-line tools

2. **QNX Target/VM Configuration**
   - VMware Workstation Pro (or VirtualBox)
   - QNX Neutrino 8.0 RTOS image
   - **Critical**: Start pathname manager in VM:
     ```bash
     devc-name /dev/name &
     ```
   - Verify network connectivity between host and VM

3. **Architecture Support**
   - x86_64 target architecture (configured in Momentics)
   - GCC compiler toolchain (included with SDP)

### Step-by-Step Build Process

#### **Method 1: Using Momentics IDE (Recommended)**

1. **Launch Momentics IDE**


2. **Import Server Project**
- `File → Import → General → Existing Projects into Workspace`
- Browse to: `qnx_encrypt_server/` folder
- Click **Finish**

3. **Import Client Project**
- Repeat Step 2 for: `qnx_encrypt_client/` folder

4. **Verify Build Configuration**
- Right-click `qnx_encrypt_server` → `Build Configurations → Set Active → Debug`
- Right-click `qnx_encrypt_client` → `Build Configurations → Set Active → Debug`

5. **Build Server**
- Right-click `qnx_encrypt_server` → `Build Project`
- **OR** press `Ctrl+B` (with project selected)
- **Expected Output**:
  ```
  **** Build of configuration Debug for project qnx_encrypt_server ****
  make -j8 all
  Building file: ../src/server.c
  Invoking: QNX Compiler
  qcc -Vgcc_ntox86_64 -c -g -O0 -fno-builtin -Wall -Wextra \
      -I"/path/to/qnx_encrypt_server/src" -MMD -MP -MF"src/server.d" \
      -MT"build/Debug/x86_64/src/server.o" -o "build/Debug/x86_64/src/server.o" \
      "../src/server.c"
  Building target: build/Debug/x86_64/qnx_encrypt_server
  Invoking: QNX C Linker
  qcc -Vgcc_ntox86_64 -L"/path/to/libs" -o "build/Debug/x86_64/qnx_encrypt_server" \
      "build/Debug/x86_64/src/server.o" -lpthread
  Finished building target
  **** Build Finished ****
  ```

6. **Build Client**
- Right-click `qnx_encrypt_client` → `Build Project`
- Verify no errors in Console view

7. **Verify Binaries**
- Server: `qnx_encrypt_server/build/x86_64-debug/qnx_encrypt_server`
- Client: `qnx_encrypt_client/build/x86_64-debug/qnx_encrypt_client`

#### **Method 2: Command Line (Advanced)**

```bash
# Navigate to server directory
cd qnx_encrypt_server

# Build server
make clean && make

# Navigate to client directory
cd ../qnx_encrypt_client

# Build client
make clean && make

# Verify binaries exist
ls -lh build/x86_64-debug/
# Output:
# -rwxr-xr-x 1 user user 45K May 1 14:30 qnx_encrypt_client
# -rwxr-xr-x 1 user user 48K May 1 14:30 qnx_encrypt_server
