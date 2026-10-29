#pragma once

#include <atomic>
#include <filesystem>
#include <string>

// Outils portables (archives officielles) ranges dans <hub>/tools : sur un PC
// sans droits admin on ne peut rien installer, le hub les telecharge a cote
// de lui. Leurs bin/ sont ajoutes au PATH du hub, donc aussi a celui de
// l'editeur qu'il lance (l'editeur les ajoute aussi lui-meme).
//
//   CMake    -> tools/cmake  (Windows et Linux)
//   Compiler -> tools/mingw  (Windows : WinLibs GCC + mingw32-make)
namespace PortableTools {

enum class Tool { CMake, Compiler };

// Un portable existe pour cet outil sur cet OS.
bool IsSupported(Tool _tool);
bool IsInstalled(Tool _tool);

// Ajoute en tete du PATH du processus le bin/ de chaque outil installe.
void AddToPath();

// Telecharge + extrait. Bloquant : a appeler depuis un thread.
// _percent avance de _from a _to pendant le telechargement.
// _status passe a _extractStatus pendant l'extraction (si non nul).
// Retourne "" si ok, sinon le message d'erreur.
std::string Install(Tool _tool, std::atomic<int> &_percent, int _from, int _to,
                    std::atomic<const char *> *_status = nullptr,
                    const char *_extractStatus = nullptr);

}
