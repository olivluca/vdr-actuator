/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 * Russian: Vyacheslav Dikonov <sdiconov@mail.ru>
 *
 * $Id: 
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "Drive East",
    "Nach Osten",
    "",// TODO
    "Muove a Est",
    "",// TODO
    "",// TODO
    "Aller Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "Mueve a Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "+ï ð÷ÀÁ÷ç",
#endif
  },
  { "Halt",
    "Anhalten",
    "",// TODO
    "Ferma",
    "",// TODO
    "",// TODO
    "Arrêt",
    "",// TODO
    "",// TODO
    "",// TODO
    "Para",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "àÁ÷ú",
#endif
  },
  { "Drive West",
    "Nach Westen",
    "",// TODO
    "Muove a Ovest",
    "",// TODO
    "",// TODO
    "Aller Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "Mueve a Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "+ï âïúïò",
#endif
  },
  { "Recalc",
    "Recalc",
    "",// TODO
    "Ricalcola",
    "",// TODO
    "",// TODO
    "Recalc",
    "",// TODO
    "",// TODO
    "",// TODO
    "Recalcula",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "©óÞóÀÃüÁïÁË",
#endif
  },
  { "Goto %d",
    "Gehe zu %d",
    "",// TODO
    "Va a %d",
    "",// TODO
    "",// TODO
    "Aller à %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ir a %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "©óÞóûÁü %d",
#endif
  },
  { "Store",
    "Speichere",
    "",// TODO
    "Memorizza",
    "",// TODO
    "",// TODO
    "Enregistrer",
    "",// TODO
    "",// TODO
    "",// TODO
    "Memoriza",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "Tïú÷ýùüÁË",
#endif
  },
  { "%d Steps East",
    "%d Schritte Osten",
    "",// TODO
    "%d Passi Est",
    "",// TODO
    "",// TODO
    "%d Incrément Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Pasos Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "%d Õïñ(÷ð) ùï ð÷ÀÁ÷ç",
#endif
  },
  { "%d Steps West",
    "%d Schritte Westen",
    "",// TODO
    "%d Passi Ovest",
    "",// TODO
    "",// TODO
    "%d Incrément Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "%d Pasos Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "Õïñ(÷ð) ùï âïúïò",
#endif
  },
  { "Set East Limit",
    "Ost-Limit setzen",
    "",// TODO
    "Imposta Limite Est",
    "",// TODO
    "",// TODO
    "Fixer Limite Est",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta Limite Este",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "¦÷ÀÁ÷ÃùÊû úÞóòóø",
#endif
  },
  { "Set Zero",
    "",// TODO
    "",// TODO
    "Messa a zero",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Puesta a cero",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Set West Limit",
    "West-Limit setzen",
    "",// TODO
    "Imposta Limite Ovest",// TODO
    "",// TODO
    "",// TODO
    "Fixer Limite Ouest",
    "",// TODO
    "",// TODO
    "",// TODO
    "Ajusta Limite Oeste",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "TïúïòùÊû úÞóòóø",
#endif
  },
  { "Frequency:",
    "Frequenz:",
    "",// TODO
    "Frequenza:",
    "",// TODO
    "",// TODO
    "Fréquence:",
    "",// TODO
    "",// TODO
    "",// TODO
    "Frecuencia:",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "ãïÀÁ÷Áï:",
#endif
  },
  { "Symbolrate:",
    "Symbolrate:",
    "",// TODO
    "Symbolrate:",
    "",// TODO
    "",// TODO
    "SymbolRate",
    "",// TODO
    "",// TODO
    "",// TODO
    "SymbolRate",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "àüýð. Àç÷Þ÷ÀÁË",
#endif
  },
  { "Scan Transponder",
    "Scan Transponder",
    "",// TODO
    "Scan Transponder",
    "",// TODO
    "",// TODO
    "Scanner Transpondeur",
    "",// TODO
    "",// TODO
    "",// TODO
    "Scan Transpondedor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "àüýð. Àç÷Þ÷ÀÁË",
#endif
  },
  { "Enable Limits",
    "Limits ein",
    "",// TODO
    "Attiva Limiti",// TODO
    "",// TODO
    "",// TODO
    "Activer Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "Activa Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "¦çøÍÃüÁË úÞóòóøÊ",
#endif
  },
  { "Disable Limits",
    "Limits aus",
    "",// TODO
    "Disattiva Limiti",
    "",// TODO
    "",// TODO
    "Désactiver Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "Desactiva Limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "+ÁçøÍÃüÁË úÞóòóøÊ",
#endif
  },
  { "Dish target: %d, position: %d",
    "",// TODO
    "",// TODO
    "Obbiettivo: %d, posizione: %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Destino disco: %d, posición: %d",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Position reached",
    "",// TODO
    "",// TODO
    "Posizione raggiunta",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posición alcanzada",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Motor wait",
    "",// TODO
    "",// TODO
    "Attesa motore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Espera motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Motor error",
    "",// TODO
    "",// TODO
    "Errore motore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Error motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Position not set",
    "",// TODO
    "",// TODO
    "Posizione non impostata",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Posición no ajustada",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Not positioned",
    "",// TODO
    "",// TODO
    "Non posizionato",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "No posicionado",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Dish at limits",
    "",// TODO
    "",// TODO
    "Raggiunto limite",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Limite alcanzado",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Position outside limits",
    "",// TODO
    "",// TODO
    "Posizione fuori dai limiti",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Fuera de los limites",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
//Setup
  { "Card, connected with motor",
    "Karte, die mit Motor verbunden ist ",
    "",// TODO
    "Scheda collegata al motore",
    "",// TODO
    "",// TODO
    "Carte, connectée au moteur",
    "",// TODO
    "",// TODO
    "",// TODO
    "Tarjeta conectada al motor",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "¦ïÞÁï À÷óòüùßùùïÎ À ú÷òðóÀ÷ý",
#endif
  },
  { "Are you sure?",
    "Sind sie sicher?",
    "",// TODO
    "Sei sicuro?",
    "",// TODO
    "",// TODO
    "Etes vous sur?",
    "",// TODO
    "",// TODO
    "",// TODO
    "¿Está seguro?",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "¦Ê ÖðóÞóùÊ?",
#endif
  },
  { "Actuator",
    "",// TODO
    "",// TODO
    "Attuatore",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Actuador",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { "Linear or h-h actuator control",
    "",// TODO
    "",// TODO
    "Controlla un attuatore lineare o h-h",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "Controla un actuador lineal o h-h",
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
    "",// TODO
#if VDRVERSNUM >= 10302
    "",// TODO
#endif
  },
  { NULL }
  };
