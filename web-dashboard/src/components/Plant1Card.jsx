import React from 'react';
import { Beaker, Droplets, Settings, Zap, Info } from 'lucide-react';

// Threshold dari Master config.h — harus sinkron
const PH_SAFE_MIN = 6.8;
const PH_SAFE_MAX = 7.2;
const PH_FAULT_LOW = 5.5;
const PH_FAULT_HIGH = 9.5;
const MIN_DOSING_PCT = 80;

function getStateReason(state, ph, levelPct, acidOn, baseOn, mixerOn) {
  switch (state) {
    case 'INITIAL_WAIT':
      return `Menunggu stabilisasi sensor awal (3 menit) sebelum memulai kontrol`;
    case 'MONITORING':
      if (levelPct < MIN_DOSING_PCT)
        return `Level terlalu rendah (${levelPct.toFixed(1)}% < ${MIN_DOSING_PCT}%) — dosing ditunda`;
      if (ph < PH_FAULT_LOW || ph > PH_FAULT_HIGH)
        return `pH ekstrem (${ph.toFixed(2)}) — menunggu evaluasi fault`;
      if (ph >= PH_SAFE_MIN && ph <= PH_SAFE_MAX)
        return `Menunggu level stabil selama 1 menit (pH=${ph.toFixed(2)}, level=${levelPct.toFixed(1)}%)`;
      if (ph < PH_SAFE_MIN)
        return `pH=${ph.toFixed(2)} < ${PH_SAFE_MIN} (asam) — menunggu level stabil untuk dosing basa`;
      if (ph > PH_SAFE_MAX)
        return `pH=${ph.toFixed(2)} > ${PH_SAFE_MAX} (basa) — menunggu level stabil untuk dosing asam`;
      return `Memantau pH=${ph.toFixed(2)}, level=${levelPct.toFixed(1)}%`;

    case 'DOSE_PULSE':
      if (baseOn) return `Pompa BASA menyala — pH=${ph.toFixed(2)} terlalu asam`;
      if (acidOn) return `Pompa ASAM menyala — pH=${ph.toFixed(2)} terlalu basa`;
      return `Dosing pulse aktif — pH=${ph.toFixed(2)}`;

    case 'DOSE_DELAY':
      return `Delay sebelum mixer menyala — menunggu larutan tersebar`;

    case 'MIXING':
      return `Mixer ON — mengaduk larutan agar pH homogen`;

    case 'SETTLING':
      return `Settling — menunggu pH stabil setelah mixing (pH=${ph.toFixed(2)})`;

    case 'PH_OK':
      return `pH dalam range aman: ${ph.toFixed(2)} (${PH_SAFE_MIN}–${PH_SAFE_MAX})`;

    case 'PH_FAULT':
      return `FAULT — pH=${ph.toFixed(2)} di luar batas atau max siklus dosing tercapai`;

    default:
      return `State: ${state}`;
  }
}

function getStateColor(state) {
  switch (state) {
    case 'INITIAL_WAIT': return '#64748b'; // Slate gray
    case 'PH_OK': return '#22c55e';
    case 'MONITORING': return '#3b82f6';
    case 'DOSE_PULSE': return '#f59e0b';
    case 'DOSE_DELAY': return '#f59e0b';
    case 'MIXING': return '#8b5cf6';
    case 'SETTLING': return '#06b6d4';
    case 'PH_FAULT': return '#ef4444';
    default: return '#94a3b8';
  }
}

const Plant1Card = ({ data, mtuState, publishCommand }) => {
  const { ph, levelPct, levelL, valveOpen, mixerOpen, acidOpen, baseOpen } = data;
  const reason = getStateReason(mtuState, ph || 0, levelPct || 0, acidOpen, baseOpen, mixerOpen);
  const stateColor = getStateColor(mtuState);

  return (
    <div className="glass-panel">
      <div className="flex items-center justify-between">
        <h2><Beaker className="text-purple" /> Plant 1 (Mixing Tank)</h2>
        <div className="flex items-center gap-2">
          <span className="text-muted text-sm">MTU State:</span>
          <select 
            className="ctrl-btn"
            value={mtuState}
            onChange={(e) => publishCommand('wwtp/mtu/cmd', { state: e.target.value })}
            style={{ fontSize: '0.8rem', padding: '0.2rem', backgroundColor: 'rgba(255, 255, 255, 0.1)' }}
          >
            <option value="INITIAL_WAIT">INITIAL_WAIT</option>
            <option value="MONITORING">MONITORING</option>
            <option value="DOSE_PULSE">DOSE_PULSE</option>
            <option value="DOSE_DELAY">DOSE_DELAY</option>
            <option value="MIXING">MIXING</option>
            <option value="SETTLING">SETTLING</option>
            <option value="PH_OK">PH_OK</option>
          </select>
        </div>
      </div>

      {/* State Reason Banner */}
      <div className="state-reason" style={{
        marginTop: '0.75rem',
        padding: '0.5rem 0.75rem',
        borderRadius: '8px',
        background: `${stateColor}15`,
        borderLeft: `3px solid ${stateColor}`,
        fontSize: '0.82rem',
        color: '#e2e8f0',
        display: 'flex',
        alignItems: 'flex-start',
        gap: '0.5rem'
      }}>
        <Info size={14} style={{ color: stateColor, marginTop: '2px', flexShrink: 0 }} />
        <span>{reason}</span>
      </div>
      
      <div className="flex gap-6 mt-6">
        <div className="level-bar-container">
          <div 
            className={`level-bar-fill ${levelPct > 85 ? 'high' : ''}`}
            style={{ height: `${Math.min(100, Math.max(0, levelPct))}%` }}
          ></div>
        </div>

        <div className="flex-col justify-between" style={{ flex: 1 }}>
          <div className="grid-2">
            <div className="metric-card">
              <h3>pH Level</h3>
              <div className="flex items-center gap-2">
                <span className="metric-value text-purple">{ph ? ph.toFixed(2) : '0.00'}</span>
              </div>
            </div>
            <div className="metric-card">
              <h3>Volume</h3>
              <div className="flex items-center gap-2">
                <span className="metric-value">{levelL ? levelL.toFixed(1) : '0.0'}</span>
                <span className="metric-unit">L ({levelPct ? levelPct.toFixed(0) : '0'}%)</span>
              </div>
            </div>
          </div>

          <div className="actuator-list mt-4">
            <div className="actuator-item flex items-center justify-between">
              <span className="flex items-center gap-2 text-muted"><Droplets size={16}/> Outlet Valve</span>
              <div className="flex items-center gap-2">
                <div className={`status-badge ${valveOpen ? 'connected' : 'disconnected'}`}>
                  {valveOpen ? 'OPEN' : 'CLOSED'}
                </div>
                <button 
                  className="ctrl-btn" 
                  onClick={() => publishCommand('wwtp/plant1/valve/cmd', { state: valveOpen ? 'CLOSED' : 'OPEN' })}>
                  Toggle
                </button>
              </div>
            </div>
            <div className="actuator-item flex items-center justify-between">
              <span className="flex items-center gap-2 text-muted"><Settings size={16}/> Mixer</span>
              <div className="flex items-center gap-2">
                <div className={`status-badge ${mixerOpen ? 'connected' : 'disconnected'}`}>
                  {mixerOpen ? 'RUNNING' : 'STOPPED'}
                </div>
                <button 
                  className="ctrl-btn" 
                  onClick={() => publishCommand('wwtp/plant1/mixer/cmd', { cmd: mixerOpen ? 'OFF' : 'ON' })}>
                  Toggle
                </button>
              </div>
            </div>
            <div className="actuator-item flex items-center justify-between">
              <span className="flex items-center gap-2 text-muted"><Zap size={16}/> Acid Pump</span>
              <div className="flex items-center gap-2">
                <div className={`status-badge ${acidOpen ? 'connected' : 'disconnected'}`}>
                  {acidOpen ? 'ON' : 'OFF'}
                </div>
                <button 
                  className="ctrl-btn" 
                  onClick={() => publishCommand('wwtp/plant1/dosing/acid/cmd', { cmd: acidOpen ? 'OFF' : 'ON' })}>
                  Toggle
                </button>
              </div>
            </div>
            <div className="actuator-item flex items-center justify-between">
              <span className="flex items-center gap-2 text-muted"><Zap size={16}/> Base Pump</span>
              <div className="flex items-center gap-2">
                <div className={`status-badge ${baseOpen ? 'connected' : 'disconnected'}`}>
                  {baseOpen ? 'ON' : 'OFF'}
                </div>
                <button 
                  className="ctrl-btn" 
                  onClick={() => publishCommand('wwtp/plant1/dosing/base/cmd', { cmd: baseOpen ? 'OFF' : 'ON' })}>
                  Toggle
                </button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Plant1Card;
