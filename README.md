## **Build Instructions**

##### **Using GCC (MinGW / TDM-GCC)**

Compile:

```bash
gcc -municode -o clipfilter.exe main.c -lole32 -luuid -luser32 -lshell32
```

Build **without a console window** (Windows GUI subsystem):

```bash
gcc -municode -mwindows -o clipfilter.exe main.c -lole32 -luuid -luser32 -lshell32
```

---

##### **Using MSVC (cl.exe)**

Compile:

```cmd
cl /Fe:clipfilter.exe main.c user32.lib shell32.lib ole32.lib uuid.lib
```

Build **without console** (GUI subsystem):

```cmd
cl /Fe:clipfilter.exe main.c user32.lib shell32.lib ole32.lib uuid.lib ^
    /link /SUBSYSTEM:WINDOWS
```

---

## Configuration

ClipboardFilter uses a simple text-based configuration file to determine which phrases should be removed from any copied text.

### **Filter File (`filter.txt`)**

Create a file named **`filter.txt`** in the same directory as `clipfilter.exe`.

Each line in this file represents a phrase to omit from clipboard content.
Blank lines are ignored.

**Example Usage:**

* **filter.txt:**
  
  ```
  you are
  bad
  ```

* **Copied text:**
  
  ```
  you are bad at this
  ```

* **Result:**
  
  ```
  at this
  ```

### **Updating Filters**

To change filtering behavior, simply edit `filter.txt` and restart the application, or right-click the tray-icon to open the config file (filter.txt).
