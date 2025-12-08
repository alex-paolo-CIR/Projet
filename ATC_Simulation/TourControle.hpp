#pragma once
#include "Commun.hpp"
#include "Journaliseur.hpp"
#include <mutex>
#include <vector>
#include <atomic>
#include <queue>
#include <chrono>

using namespace std;

// demande piste avec priorite
struct DemandePiste {
    int idAvion;
    bool estUrgence;
    chrono::steady_clock::time_point momentDemande;
    
    DemandePiste(int id, bool urgence)
        : idAvion(id)
     , estUrgence(urgence)
        , momentDemande(chrono::steady_clock::now())
  {}
    
    // comparaison pour file priorite
    bool operator<(const DemandePiste& autre) const {
      // urgences en premier
        if (estUrgence != autre.estUrgence) {
          return !estUrgence;
 }
   // sinon fifo
   return momentDemande > autre.momentDemande;
    }
};

// controle aerien
class TourControle {
public:
    TourControle(int nombrePlacesParking = 5);
    ~TourControle() = default;

    // gestion piste avec priorite
 bool demanderPiste(int idAvion, bool estUrgence = false);
    void libererPiste(int idAvion);
    bool pisteOccupee() const;
    
    // fermeture ouverture piste
    void fermerPiste();
    void ouvrirPiste();
    bool pisteFermee() const;

    // gestion parkings
    int assignerParking(int idAvion);
    void libererParking(int idAvion, int idParking);
    Position obtenirPositionParking(int idParking) const;
    
    // positions piste
    Position obtenirPositionPiste() const { return positionPiste_; }
    Position obtenirSeuilPiste() const { return seuilPiste_; }
    Position obtenirPointApproche() const { return pointApproche_; }
    Position obtenirPointDepart() const { return pointDepart_; }

    // statistiques
    int obtenirTotalAtterrissages() const { return totalAtterrissages_.load(); }
    int obtenirTotalDecollages() const { return totalDecollages_.load(); }
    int obtenirTotalCrashs() const { return totalCrashs_.load(); }
    int obtenirAvionsEnAttente() const;

private:
    bool essayerAccorderPiste(int idAvion);
  
    mutable mutex mutexPiste_;
    atomic<bool> pisteOccupee_;
    atomic<bool> pisteFermee_;
    int pisteOccupeePar_;
    priority_queue<DemandePiste> fileAttentePiste_;

    mutable mutex mutexParking_;
    vector<bool> parkingOccupe_;
    vector<Position> positionsParking_;
    
    Position positionPiste_;
    Position seuilPiste_;
    Position pointApproche_;
    Position pointDepart_;

    atomic<int> totalAtterrissages_;
    atomic<int> totalDecollages_;
    atomic<int> totalCrashs_;
};
