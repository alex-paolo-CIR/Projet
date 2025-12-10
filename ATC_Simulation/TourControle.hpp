#pragma once
#include "Commun.hpp"
#include "Journaliseur.hpp"
#include <mutex>
#include <vector>
#include <atomic>
#include <queue>
#include <deque>
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

// controle aerien avec deux pistes
class TourControle {
public:
    TourControle(int nombrePlacesParking = 5);
    ~TourControle() = default;

    // gestion piste gauche atterrissage avec priorite
    bool demanderPisteGauche(int idAvion, bool estUrgence = false);
    void libererPisteGauche(int idAvion);
    bool pisteGaucheOccupee() const;
    
    // gestion piste droite decollage
    bool demanderPisteDroite(int idAvion); // obsolete, garder pour compatibilite
    void rejoindreFileDecollage(int idAvion);
    int obtenirRangDecollage(int idAvion) const;
    bool accorderDecollage(int idAvion);
    void libererPisteDroite(int idAvion);
    bool pisteDroiteOccupee() const;
    
 // fermeture ouverture pistes
    void fermerPistes();
    void ouvrirPistes();
    bool pistesFermees() const;

    // gestion parkings
    int assignerParking(int idAvion);
    void libererParking(int idAvion, int idParking);
    Position obtenirPositionParking(int idParking) const;
    
    // positions piste gauche atterrissage
    Position obtenirPositionPisteGauche() const { return positionPisteGauche_; }
    Position obtenirSeuilPisteGauche() const { return seuilPisteGauche_; }
    Position obtenirFinPisteGauche() const { return finPisteGauche_; }
    Position obtenirPointApproche() const { return pointApproche_; }
  
    // positions piste droite decollage
    Position obtenirPositionPisteDroite() const { return positionPisteDroite_; }
    Position obtenirSeuilPisteDroite() const { return seuilPisteDroite_; }
    Position obtenirFinPisteDroite() const { return finPisteDroite_; }
    Position obtenirPointDepart() const { return pointDepart_; }
  
    // point croisement entre les deux pistes
    Position obtenirPointCroisement() const { return pointCroisement_; }

    // statistiques
    int obtenirTotalAtterrissages() const { return totalAtterrissages_.load(); }
    int obtenirTotalDecollages() const { return totalDecollages_.load(); }
    int obtenirTotalCrashs() const { return totalCrashs_.load(); }
    int obtenirAvionsEnAttente() const;

private:
    bool essayerAccorderPisteGauche(int idAvion);
    bool essayerAccorderPisteDroite(int idAvion);
  
    // gestion piste gauche atterrissage
 mutable mutex mutexPisteGauche_;
    atomic<bool> pisteGaucheOccupee_;
    int pisteGaucheOccupeePar_;
    priority_queue<DemandePiste> fileAttentePisteGauche_;
    
    // gestion piste droite decollage
    mutable mutex mutexPisteDroite_;
    atomic<bool> pisteDroiteOccupee_;
    int pisteDroiteOccupeePar_;
    deque<int> fileAttentePisteDroite_;
    
    atomic<bool> pistesFermees_;

    // gestion parkings
    mutable mutex mutexParking_;
    vector<bool> parkingOccupe_;
    vector<Position> positionsParking_;
    
    // positions piste gauche atterrissage
    Position positionPisteGauche_;
    Position seuilPisteGauche_;
    Position finPisteGauche_;
    Position pointApproche_;
    
    // positions piste droite decollage
    Position positionPisteDroite_;
    Position seuilPisteDroite_;
    Position finPisteDroite_;
    Position pointDepart_;
    
    // point critique croisement
    Position pointCroisement_;

    atomic<int> totalAtterrissages_;
    atomic<int> totalDecollages_;
    atomic<int> totalCrashs_;
};
