# Rootkit Windows (Projet Éducatif)

## ⚠️ Avertissement Important / Disclaimer ⚠️

Ce projet est développé à des fins strictement éducatives, dans le but de comprendre les mécanismes internes de Windows, la programmation de pilotes et les techniques de type rootkit (comme la manipulation directe d'objets noyau - DKOM).

**L'utilisation de rootkits ou de techniques similaires peut être illégale et nuisible si employée à des fins malveillantes.** Ce code est fourni "tel quel", sans garantie. L'auteur n'est pas responsable des dommages ou des conséquences découlant de l'utilisation (ou de la mauvaise utilisation) de ce projet.

**Utilisez ce projet de manière responsable, dans un environnement de test contrôlé (comme une machine virtuelle), et ne l'exécutez jamais sur un système que vous n'êtes pas autorisé à modifier ou sur lequel vous ne souhaitez pas prendre de risques.**

## Description

Ce projet est un exemple basique de rootkit pour Windows, accompagné d'une application compagnon en mode utilisateur. Il démontre des techniques telles que l'élévation de privilèges par vol de token et la dissimulation de processus par manipulation directe des structures du noyau (DKOM).

L'objectif principal est l'apprentissage et l'expérimentation des aspects de bas niveau de Windows.

## Fonctionnalités

### Rootkit (Pilote Kernel - `rootkit_win/rootkit.c`)
* **Élévation de Privilèges :** Permet d'élever les privilèges d'un processus au niveau `SYSTEM` en copiant le token du processus Système.
* **Dissimulation de Processus :** Permet de cacher/réafficher des processus en les déliant/reliant de la liste `ActiveProcessLinks` de la structure `EPROCESS`.
* Communication avec une application en mode utilisateur via IOCTL.

### Compagnon (Application User-mode - `rootkit_win/compagnon.c`)
* Interface en ligne de commande pour interagir avec le pilote rootkit.
* Option d'auto-installation (`--install`) :
    * Copie `compagnon.exe` dans `%LOCALAPPDATA%\MonCompagnonApp`.
    * Ajoute ce répertoire au `PATH` de l'utilisateur pour un lancement facile.
* Fonctionnalités du menu :
    1.  Élever les privilèges du processus compagnon lui-même.
    2.  Cacher un processus spécifié par son PID.
    3.  Rendre visible un processus précédemment caché, spécifié par son PID.
    4.  Lister les processus actuellement cachés par le rootkit.
* Tentative de masquage de lui-même au démarrage (via le pilote).

## Structure du Projet

```text
<racine_du_projet>/
├── rootkit_win/
│   ├── compagnon.c       # Code source de l'application compagnon
│   ├── compagnon.exe     # Exécutable compilé du compagnon
│   ├── compagnon.obj     # Fichier objet intermédiaire
│   ├── driver.vcxproj    # Fichier de projet Visual Studio pour le pilote
│   ├── rootkit.c         # Code source du pilote rootkit
│   └── x64/              # Répertoire de sortie de compilation pour le pilote (ex: rootkit.sys)
└── scripts/
    ├── ConfigWindows_DevOff.cmd  # Désactive la configuration de développement (ex: test signing)
    ├── ConfigWindows_DevOn.cmd   # Active la configuration de développement (ex: test signing)
    ├── LoadDriver.cmd            # Charge le pilote rootkit.sys
    ├── LoadFilt.cmd              # (Script pour charger un pilote filtre - à clarifier si utilisé)
    ├── UnloadDriver.cmd          # Décharge et supprime le service du pilote rootkit
    └── UnloadFilt.cmd            # (Script pour décharger un pilote filtre - à clarifier si utilisé)
PrérequisSystème d'Exploitation : Windows [Indiquez ici la version de Windows sur laquelle vous avez testé, ex: Windows 10 x64 Build XXXX].IMPORTANT : Les offsets du noyau (comme EPROCESS_TOKEN_OFFSET = 0x4B8, EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET = 0x448 dans rootkit.c) sont extrêmement spécifiques à la version et au build de Windows. Utiliser des offsets incorrects mènera à un plantage système (BSOD). Vous devrez vérifier et ajuster ces offsets pour votre version cible de Windows.Environnement de compilation pour le pilote :EWDK (Enterprise Windows Driver Kit) : Un environnement en ligne de commande qui contient les compilateurs, les outils de liaison, le SDK Windows et le WDK.Compilateur pour le compagnon :Compilateur C Microsoft (MSVC, cl.exe), généralement disponible via Visual Studio ou les outils de build Visual Studio.Mode Test (Test Signing) : Pour charger des pilotes non signés officiellement, Windows doit être en mode "test signing". Les scripts ConfigWindows_DevOn.cmd peuvent aider à configurer cela.Compilation1. Pilote (rootkit.sys)Assurez-vous d'avoir l'EWDK configuré et accessible.Ouvrez une invite de commandes de l'EWDK.Naviguez vers le répertoire contenant driver.vcxproj (c'est-à-dire rootkit_win/).Exécutez la commande de build avec MSBuild :msbuild driver.vcxproj /t:clean /t:build /p:Platform=x64 /p:Configuration=Release
# Vous pouvez remplacer Release par Debug si nécessaire
Le pilote rootkit.sys (ou un nom similaire basé sur votre projet) devrait être généré, typiquement dans un sous-répertoire de rootkit_win/x64/ (comme Release ou Debug).2. Compagnon (compagnon.exe)Ouvrez une invite de commandes pour développeurs Visual Studio (Developer Command Prompt for VS) ou une invite où cl.exe est dans le PATH.Naviguez vers le répertoire rootkit_win/.Compilez avec la commande :cl.exe compagnon.c /Fecompagnon.exe
(Les #pragma comment(lib, ...) dans compagnon.c devraient gérer les dépendances comme Psapi.lib, Advapi32.lib, User32.lib).Installation et UtilisationIL EST FORTEMENT RECOMMANDÉ D'EFFECTUER TOUTES CES OPÉRATIONS DANS UNE MACHINE VIRTUELLE DÉDIÉE AU TEST.1. Préparer l'Environnement WindowsExécutez scripts/ConfigWindows_DevOn.cmd en tant qu'administrateur pour activer le mode "test signing" (signature de test des pilotes).cd scripts
ConfigWindows_DevOn.cmd
Un redémarrage de Windows sera probablement nécessaire.2. Compiler les ComposantsSuivez les étapes de la section Compilation.Assurez-vous d'avoir le fichier .sys de votre pilote (ex: rootkit.sys) et compagnon.exe.3. Charger le PiloteLe script LoadDriver.cmd est utilisé pour cette tâche. Il prend le chemin du fichier .sys en argument.Contenu de scripts/LoadDriver.cmd :@echo off
set sysfile=%~f1
set drvname=%~n1

echo LOAD DRVNAME %drvname% for file %sysfile%

echo STOP/DELETE POTENTIAL PREVIOUS SERVICE
sc.exe stop %drvname% >nul 2>&1
sc.exe delete %drvname% >nul 2>&1

echo COPY DRIVER IN SYSTEM32\DRIVERS
echo copy "%sysfile%" c:\windows\system32\drivers\
copy "%sysfile%" c:\windows\system32\drivers\
if errorlevel 1 (
    echo ERREUR: La copie du pilote a echoue. Verifiez les droits d'administrateur.
    goto :eof
)

echo CREATE SERVICE DEVICE
sc.exe create %drvname% type= kernel start= demand error= normal binPath= c:\windows\system32\drivers\%~nx1 DisplayName= %drvname%
if errorlevel 1 (
    echo ERREUR: La creation du service a echoue.
    goto :eof
)

echo START SERVICE DEVICE
sc.exe start %drvname%
if errorlevel 1 (
    echo ERREUR: Le demarrage du service a echoue.
)
echo.
Ouvrez une invite de commandes en tant qu'administrateur.Naviguez vers le répertoire scripts/.Exécutez LoadDriver.cmd en passant le chemin complet de votre fichier .sys compilé :cd scripts
LoadDriver.cmd ..\rootkit_win\x64\Release\rootkit.sys 
# Adaptez le chemin et le nom (rootkit.sys) vers votre pilote compilé.
Ce script va :Tenter d'arrêter et de supprimer un service existant du même nom.Copier votre fichier .sys dans C:\Windows\System32\drivers\.Créer un service pour le pilote (nommé d'après le fichier, ex: "rootkit") avec démarrage "demand".Démarrer le service.4. Installer et Exécuter le CompagnonPremière installation (optionnel mais recommandé pour la facilité d'accès) :Ouvrez une invite de commandes normale (pas besoin d'admin pour cette étape si APP_EXECUTABLE_NAME dans compagnon.c correspond bien à compagnon.exe).Naviguez vers rootkit_win/.Exécutez :compagnon.exe --install
Cela copiera compagnon.exe dans %LOCALAPPDATA%\MonCompagnonApp et ajoutera ce dossier à votre PATH utilisateur.Ouvrez une nouvelle invite de commandes pour que les changements du PATH soient pris en compte.Lancer le Compagnon :Si vous avez fait l'étape --install, ouvrez une nouvelle invite de commandes et tapez simplement :compagnon
Sinon, naviguez vers rootkit_win/ et exécutez compagnon.exe.Menu des Options du CompagnonUne fois le compagnon lancé et connecté au pilote :Élévation de privilège du processus compagnon : Tente d'obtenir les privilèges SYSTEM.Cacher un autre processus (via PID) : Demande un PID à cacher.Rendre visible un processus (via PID) : Demande un PID à rendre visible.Lister les processus cachés : Affiche les PIDs des processus actuellement cachés par le pilote.Aperçu des Scripts (scripts/)ConfigWindows_DevOn.cmd: Configure Windows pour le développement de pilotes (par exemple, active le "test signing mode" via bcdedit /set testsigning on). Nécessite des droits d'administrateur et un redémarrage.ConfigWindows_DevOff.cmd: Inverse les configurations de ConfigWindows_DevOn.cmd (par exemple, bcdedit /set testsigning off). Nécessite des droits d'administrateur et un redémarrage.LoadDriver.cmd: Script pour copier, enregistrer (service demand start) et démarrer un pilote noyau (.sys). Prend le chemin du fichier pilote en argument. Nécessite des droits d'administrateur.UnloadDriver.cmd: Script pour arrêter, supprimer le service et (tenter de) supprimer le fichier pilote de system32\drivers. Prend le nom du service (généralement le nom du fichier pilote sans extension) en argument. Nécessite des droits d'administrateur.LoadFilt.cmd / UnloadFilt.cmd: Scripts génériques potentiellement utilisés pour charger/décharger d'autres types de pilotes, comme des pilotes filtres. Leur utilité spécifique dans ce projet dépend de composants non décrits. (Vous pouvez clarifier ou supprimer cette partie si non pertinente).Considérations Importantes et AvertissementsSTABILITÉ DU SYSTÈME : La programmation de pilotes et la manipulation directe du noyau sont intrinsèquement risquées. Des erreurs peuvent facilement causer des plantages système (BSOD). Testez exclusivement sur des machines virtuelles.OFFSETS DU NOYAU : Répétons-le, les offsets des structures du noyau (EPROCESS_TOKEN_OFFSET, EPROCESS_ACTIVE_PROCESS_LINKS_OFFSET) DOIVENT correspondre à votre version exacte de Windows. Recherchez les valeurs correctes pour votre système (par exemple, en utilisant WinDbg) ou le rootkit ne fonctionnera pas et provoquera un plantage.DÉTECTION : Les techniques utilisées dans ce projet sont connues et la plupart des logiciels antivirus ou EDR (Endpoint Detection and Response) détecteront ce type d'activité.PATCHGUARD : Sur les versions x64 de Windows, PatchGuard protège les structures critiques du noyau. La modification de ces structures peut, à terme, être détectée et entraîner un plantage.ÉTHIQUE : Ce projet est à but éducatif. Ne l'utilisez jamais à des fins malveillantes.
