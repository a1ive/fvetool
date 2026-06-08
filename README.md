# fvetool

fvetool (FVEUnlocker) is a tool for managing Windows BitLocker (Full Volume Encryption) drives. It is available as both a Command Line Interface (CLI) and a Graphical User Interface (GUI).

## Warning

This software utilizes undocumented and non-public Windows APIs from the internal BitLocker library (`fveapi.dll`). 

Please be aware:
- Microsoft does not support or document these APIs, and their behavior may change or become unstable in future Windows updates.
- Using this tool may result in unexpected behavior, system instability, or permanent data loss.
- Always back up your critical data before using this utility. Use it entirely at your own risk.

## Features

- Query and display volume encryption, protection, and lock status.
- Unlock locked volumes using a password or a 48-digit recovery password.
- Lock unlocked volumes (with optional drive dismounting).
- Enable BitLocker encryption with a password and generate a recovery key.
- Disable BitLocker encryption (decrypt volume).
- Retrieve and view key protectors (requires administrator privileges).

## Command Line Interface (fvecli)

`fvecli` provides command line actions to query and control BitLocker volumes.

### Usage

```cmd
fvecli status <volume>
fvecli keys <volume>
fvecli unlock-password <volume> <password>
fvecli unlock-recovery <volume> <recovery-password>
fvecli lock <volume> [--dismount]
fvecli encrypt <volume> <password>
fvecli decrypt <volume> [flags]
fvecli off <volume> [flags]
```

### Examples

- **Check volume status:**
  ```cmd
  fvecli status C:
  ```
- **List volume key protectors:** *(Requires administrator privileges)*
  ```cmd
  fvecli keys C:
  ```
- **Unlock a volume with a password:**
  ```cmd
  fvecli unlock-password D: my-password
  ```
- **Unlock a volume with a recovery password:**
  ```cmd
  fvecli unlock-recovery D: 111111-222222-333333-444444-555555-666666-777777-888888
  ```
- **Enable BitLocker encryption:**
  ```cmd
  fvecli encrypt M: my-password
  ```
- **Disable BitLocker encryption (decrypt):**
  ```cmd
  fvecli off D:
  ```

## Graphical User Interface (fvegui)

`fvegui` is a graphical application to manage BitLocker. By default, it opens the decryption interface, but it also accepts command line parameters to navigate directly to a specific tab and select a drive.

### Usage

```cmd
fvegui [/decrypt | /encrypt | /keys | /view] <volume>
```

### Parameters

- `/decrypt <volume>`: Launches fvegui, opens the Decrypt tab, and selects the specified volume.
- `/encrypt <volume>`: Launches fvegui, opens the Encrypt tab, and selects the specified volume.
- `/keys <volume>` or `/view <volume>`: Launches fvegui, opens the View key protectors tab, and selects the specified volume.

### Examples

- **Open GUI on the encrypt tab for drive D:**
  ```cmd
  fvegui /encrypt D:
  ```
- **Open GUI on the key protectors view tab for drive D:**
  ```cmd
  fvegui /view D:
  ```

## License

This project is licensed under the GPL-3.0 License. See the LICENSE file for details.
