
        // Dedicated 4-Instance MODEP Port Routing (available globally before DOM render)
        const MODEP_PORTS = {
            main_rig: 8881, // Ch 7/8
            insert_2: 8882, // Ch 1/2
            insert_3: 8883, // Ch 3/4
            insert_4: 8884  // Ch 5/6
        };

        function getModepPortForChannel(ch) {
            if (ch === 7 || ch === 8) return MODEP_PORTS.main_rig;
            if (ch === 1 || ch === 2) return MODEP_PORTS.insert_2;
            if (ch === 3 || ch === 4) return MODEP_PORTS.insert_3;
            return MODEP_PORTS.insert_4;
        }

        function openModepInsert(ch) {
            const port = getModepPortForChannel(ch);
            const host = window.location.hostname || '192.168.1.148';
            window.open(`http://${host}:${port}`, `_modep_${port}`);
        }
    