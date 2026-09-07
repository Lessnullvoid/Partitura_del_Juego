#pragma once
#include "ofMain.h"
#include <vector>
#include <string>
#include <map>
#include <deque>

// ClipPool gestiona dos bibliotecas de clips independientes:
//   Pool vertical  (canales 0 .. kGroupThreshold-1) — escaneado de la carpeta principal.
//   Pool horizontal (canales kGroupThreshold .. 7)   — escaneado de horizontalFolder.
// Cada pool mantiene su propio seguimiento de clips activos e historial por canal
// para que los canales verticales y horizontales no interfieran en la selección.
// Si no hay carpeta horizontal configurada, el pool vertical se usa para todos los canales.
class ClipPool {
public:
    // Los canales >= este índice usan el pool horizontal (cuando está cargado).
    static constexpr int kGroupThreshold = 4;

    // Escanea la carpeta principal (vertical).
    void scan(const std::string& folder);

    // Escanea la carpeta horizontal (opcional). Llamar después de scan().
    void scanHorizontal(const std::string& folder);

    // Devuelve un clip aleatorio para channelIdx, evitando clips activos en otros
    // canales del mismo pool. Los canales 0..kGroupThreshold-1 usan el pool vertical;
    // los canales >= kGroupThreshold usan el pool horizontal (cae al vertical
    // si el pool horizontal está vacío).
    std::string getRandomClip(int channelIdx);
    std::string getIndependentClip(int channelIdx);

    // Selección de clip de evento compartido por grupo de pool.
    // group 0 = pool vertical, group 1 = pool horizontal.
    // Cae al pool vertical cuando el pool horizontal está vacío.
    std::string getSharedClip();               // compatibilidad hacia atrás: group 0
    std::string getSharedClipForGroup(int group);

    // Lo llama cada Channel después de cargar un clip.
    void setActiveClip(int channelIdx, const std::string& path);

    int  totalClips()           const { return (int)clips_.size(); }
    int  totalHorizontalClips() const { return (int)horizontalClips_.size(); }
    const std::vector<std::string>& getClips()           const { return clips_; }
    const std::vector<std::string>& getHorizontalClips() const { return horizontalClips_; }

private:
    std::string chooseAndRemember(const std::vector<std::string>& candidates,
                                  std::deque<std::string>& history);

    // Devuelve true cuando channelIdx debe usar el pool horizontal.
    bool isHorizontalChannel(int channelIdx) const;

    // Pool vertical
    std::vector<std::string>               clips_;
    std::map<int, std::string>             activeClips_;
    std::map<int, std::deque<std::string>> channelHistory_;
    std::deque<std::string>                sharedHistory_;

    // Pool horizontal (canales >= kGroupThreshold)
    std::vector<std::string>               horizontalClips_;
    std::map<int, std::string>             activeHorizontalClips_;
    std::map<int, std::deque<std::string>> horizontalChannelHistory_;
    std::deque<std::string>                horizontalSharedHistory_;
};
