import React from 'react';
import { Beaker, Droplets, Settings, Zap } from 'lucide-react';

const Plant1Card = ({ data, mtuState, publishCommand }) => {
  const { ph, levelPct, levelL, valveOpen, mixerOpen, acidOpen, baseOpen } = data;

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
            <option value="MONITORING">MONITORING</option>
            <option value="DOSE_PULSE">DOSE_PULSE</option>
            <option value="DOSE_DELAY">DOSE_DELAY</option>
            <option value="MIXING">MIXING</option>
            <option value="SETTLING">SETTLING</option>
            <option value="PH_OK">PH_OK</option>
          </select>
        </div>
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
