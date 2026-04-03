Contexte du projet – SafeFeet by Njima

Les chutes représentent un problème majeur de sécurité pour de nombreuses personnes, notamment les personnes âgées, les travailleurs évoluant sur des terrains irréguliers ou les sportifs pratiquant des activités en extérieur. Ces accidents sont souvent causés par une perte d’équilibre soudaine ou par des défauts présents sur la chaussée, tels que des trous, des obstacles ou des surfaces glissantes.

Dans ce contexte, le projet SafeFeet by Njima propose le développement d’un système embarqué intelligent intégré à des bottes biomécaniques capables de détecter, anticiper et prévenir les chutes. Ce système combine l’analyse des mouvements de l’utilisateur avec l’observation de l’environnement qui l’entoure.

Le dispositif repose sur plusieurs capteurs simulés, notamment des capteurs de mouvement comme un accéléromètre et un gyroscope permettant d’analyser l’équilibre et les déplacements de l’utilisateur. En complément, d’autres capteurs sont utilisés pour cartographier l’environnement immédiat, par exemple pour détecter des irrégularités du sol, des obstacles ou des défauts de la chaussée pouvant entraîner une chute.

Les données collectées permettent au système de prévoir les situations à risque avant qu’elles ne se produisent, en analysant à la fois la posture de l’utilisateur et les caractéristiques du terrain. Lorsqu’un danger potentiel est détecté, le système peut activer un mécanisme de stabilisation ou envoyer une alerte afin de prévenir la chute.

L’architecture du système est basée sur plusieurs tâches concurrentes exécutées en temps réel, chacune responsable d’une fonction spécifique : acquisition des données capteurs, analyse du mouvement, cartographie de l’environnement, détection de chute et stabilisation. Afin de garantir une réaction rapide face aux situations critiques, ces tâches sont exécutées selon une politique d’ordonnancement temps réel avec des priorités différentes, permettant aux fonctions de sécurité de s’exécuter en priorité.

L’objectif du projet SafeFeet by Njima est donc de concevoir un prototype logiciel simulant un système intelligent de prévention des chutes, capable d’analyser le comportement de l’utilisateur et son environnement afin d’anticiper les risques et d’améliorer la sécurité des déplacements.
