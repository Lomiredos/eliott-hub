#pragma once

#include <atomic>
#include <filesystem>
#include <string>

// CMake portable (archive officielle Kitware) range dans <hub>/tools/cmake :
// sur un PC sans droits admin on ne peut pas installer cmake, le hub le
// telecharge a cote de lui. Son bin/ est ajoute au PATH du hub, donc aussi a
// celui de l'editeur qu'il lance (bouton Build).
namespace PortableCMake {

std::filesystem::path BinDir();
bool IsInstalled();

// Ajoute BinDir() en tete du PATH du processus (si installe).
void AddToPath();

// Telecharge + extrait. Bloquant : a appeler depuis un thread.
// _percent avance de _from a _to pendant le telechargement.
// Retourne "" si ok, sinon le message d'erreur.
std::string Install(std::atomic<int> &_percent, int _from, int _to);

}
