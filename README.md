# Rootkit Windows (Projet Éducatif)

## ⚠️ Avertissement Important / Disclaimer ⚠️

Ce projet est développé à des fins **strictement éducatives**, dans le but de comprendre les mécanismes internes de Windows, la programmation de pilotes et les techniques de type rootkit (comme la manipulation directe d'objets noyau - DKOM).

> **L'utilisation de rootkits ou de techniques similaires peut être illégale et nuisible si employée à des fins malveillantes.** Ce code est fourni *"tel quel"*, sans garantie. L'auteur n'est pas responsable des dommages ou des conséquences découlant de l'utilisation (ou de la mauvaise utilisation) de ce projet.

**Utilisez ce projet de manière responsable**, dans un environnement de test contrôlé (comme une machine virtuelle), et **ne l'exécutez jamais sur un système que vous n'êtes pas autorisé à modifier** ou sur lequel vous ne souhaitez pas prendre de risques.

---

## 📃 Description

Ce projet est un exemple basique de rootkit pour Windows, accompagné d'une application compagnon en mode utilisateur. Il démontre des techniques telles que :

* ❌ Élévation de privilèges par vol de token.
* 🕵️ Dissimulation de processus via manipulation de `EPROCESS` (DKOM).
* ↔️ Communication IOCTL entre mode noyau et mode utilisateur.

L'objectif principal est **l'apprentissage et l'expérimentation des aspects de bas niveau de Windows**.

---

## ⚙️ Fonctionnalités

### Rootkit (Pilote Kernel - `rootkit_win/rootkit.c`)

* **Élévation de Privilèges :** Copie du token du processus `SYSTEM` sur un autre processus.
* **Dissimulation de Processus :** Déliaison/reliement de la liste `ActiveProcessLinks` dans `EPROCESS`.
* **Interface IOCTL :** Permet les commandes depuis le mode utilisateur.

### Compagnon (Application User-mode - `rootkit_win/compagnon.c`)

* **Interface CLI** pour contrôler le pilote.
* **Installation auto via `--install`** :

  * Copie dans `%LOCALAPPDATA%\MonCompagnonApp`
  * Ajout au `PATH` utilisateur
* **Menu interactif** :

  1. ↑ Élever les privilèges
  2. 🚫 Cacher un processus (PID)
  3. 👁️ Rendre visible un processus (PID)
  4. 📃 Lister les processus cachés

---

## 📂 Structure du Projet

```
<racine_du_projet>/
├── rootkit_win/
│   ├── compagnon.c         # Application compagnon (source)
│   ├── compagnon.exe       # Binaire compagnon
│   ├── driver.vcxproj      # Projet Visual Studio (pilote)
│   ├── rootkit.c           # Code source du pilote
│   └── x64/                # Build output (rootkit.sys)
└── scripts/
    ├── ConfigWindows_DevOn.cmd    # Active le mode test signing
    ├── ConfigWindows_DevOff.cmd   # Désactive le mode test
    ├── LoadDriver.cmd             # Charge le pilote
    ├── UnloadDriver.cmd           # Décharge le pilote
```

---

## ✅ Prérequis

* **Windows 10 x64 Build XXXX** (adapter selon version).
* **Offsets noyau à jour !**

  * Exemple dans `rootkit.c` :

    ```c
    #define EPROCESS_TOKEN_OFFSET 0x4B8
    #define EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET 0x448
    ```
  * ⚠️ Offsets spécifiques à votre build !

### Environnement de compilation

* **Pilote** : \[EWDK (Enterprise Windows Driver Kit)]
* **Compagnon** : \[MSVC `cl.exe` - via Visual Studio Developer Command Prompt]
* **Mode Test Signing activé** :

  ```cmd
  cd scripts
  ConfigWindows_DevOn.cmd
  ```

---

## 💪 Compilation

### 1. Pilote (rootkit.sys)

```cmd
cd rootkit_win
msbuild /t:clean /t:build /p:Platform=x64
```

### 2. Compagnon (compagnon.exe)

```cmd
cd rootkit_win
cl.exe compagnon.c /Fecompagnon.exe
```

---

## 🚁 Installation & Utilisation

### 1. Activer le mode test

```cmd
cd scripts
ConfigWindows_DevOn.cmd
```

> Redémarrage nécessaire

### 2. Charger le pilote

```cmd
cd scripts
LoadDriver.cmd ..\rootkit_win\x64\Release\rootkit.sys
```

### 3. Installer le compagnon (facultatif mais recommandé)

```cmd
cd rootkit_win
compagnon.exe --install
```

### 4. Lancer le compagnon

```cmd
compagnon
```

---

## 📄 Aperçu du Script LoadDriver.cmd

```cmd
@echo off
set sysfile=%~f1
set drvname=%~n1

sc stop %drvname% >nul 2>&1
sc delete %drvname% >nul 2>&1

copy "%sysfile%" C:\Windows\System32\drivers\
sc create %drvname% type= kernel start= demand binPath= C:\Windows\System32\drivers\%~nx1
sc start %drvname%
```

---

## 🔧 Menu du Compagnon

1. ↑ **Élévation de privilège** : Deviens SYSTEM
2. 🚫 **Cacher un PID**
3. 👁️ **Rendre un PID visible**
4. 📊 **Lister les processus cachés**

---

## 🔹 Remarques

* Ce projet est à but **strictement pédagogique**.
* Il est recommandé d'utiliser **une machine virtuelle** pour tous les tests.
* ⚠️ Les manipulations de structures noyau sont sensibles. Un mauvais offset = BSOD !

---

## 📖 Ressources Utiles

* [EWDK](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
* \[Structures Windows Internes (EPROCESS, etc)]
* \[IOCTL communication driver <-> userland]

---

## © Licence

Projet publié à des fins éducatives. Aucun usage en environnement de production. Respectez les lois en vigueur.
