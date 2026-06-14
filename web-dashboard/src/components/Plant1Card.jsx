import React from 'react';
import { Beaker, Droplets, Settings, Zap } from 'lucide-react';

const Plant1Card = ({ data, mtuState }) => {
  const { ph, levelPct, levelL, valveOpen, mixerOpen, acidOpen, baseOpen } = data;

  return (
    <div className="glass-panel">
      <h2><Beaker className="text-purple" /> Plant 1 (Mixing Tank)</h2>
      
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
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Droplets size={16}/> Outlet Valve</span>
              <div className={`status-badge ${valveOpen ? 'connected' : 'disconnected'}`}>
                {valveOpen ? 'OPEN' : 'CLOSED'}
              </div>
            </div>
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Settings size={16}/> Mixer</span>
              <div className={`status-badge ${mixerOpen ? 'connected' : 'disconnected'}`}>
                {mixerOpen ? 'RUNNING' : 'STOPPED'}
              </div>
            </div>
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Zap size={16}/> Acid Pump</span>
              <div className={`status-badge ${acidOpen ? 'connected' : 'disconnected'}`}>
                {acidOpen ? 'ON' : 'OFF'}
              </div>
            </div>
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Zap size={16}/> Base Pump</span>
              <div className={`status-badge ${baseOpen ? 'connected' : 'disconnected'}`}>
                {baseOpen ? 'ON' : 'OFF'}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Plant1Card;
