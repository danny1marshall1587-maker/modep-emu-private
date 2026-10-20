const REAPER_BASE = `http://${window.location.hostname}:8080`;

const ReaperTransport = {
  record: () => fetch(`${REAPER_BASE}/_ /1013`, { mode: 'no-cors' }),
  stop:   () => fetch(`${REAPER_BASE}/_ /1016`, { mode: 'no-cors' }),
  play:   () => fetch(`${REAPER_BASE}/_ /1007`, { mode: 'no-cors' }),
  marker: () => fetch(`${REAPER_BASE}/_ /40157`, { mode: 'no-cors' })
};

window.addEventListener('DOMContentLoaded', () => {
  const btnRec = document.getElementById('btn-reaper-rec');
  const btnStop = document.getElementById('btn-reaper-stop');
  const btnPlay = document.getElementById('btn-reaper-play');

  if (btnRec) {
    btnRec.addEventListener('click', () => {
      ReaperTransport.record();
      btnRec.classList.add('recording-active');
    });
  }

  if (btnStop) {
    btnStop.addEventListener('click', () => {
      ReaperTransport.stop();
      if (btnRec) btnRec.classList.remove('recording-active');
    });
  }

  if (btnPlay) {
    btnPlay.addEventListener('click', () => {
      ReaperTransport.play();
    });
  }
});
