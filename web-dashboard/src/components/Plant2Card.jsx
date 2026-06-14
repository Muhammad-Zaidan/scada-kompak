import React from 'react';
import { Waves, Activity, Droplets } from 'lucide-react';

const Plant2Card = ({ data }) => {
  const { levelPct, levelL, flowLPM, mainValve, fsValve } = data;

  return (
    <div className="glass-panel">
      <h2><Waves className="text-cyan" /> Plant 2 (Storage & Flow)</h2>
      
      <div className="flex gap-6 mt-6">
        <div className="level-bar-container">
          <div 
            className={`level-bar-fill ${levelPct > 80 ? 'high' : ''}`}
            style={{ height: `${Math.min(100, Math.max(0, levelPct))}%` }}
          ></div>
        </div>

        <div className="flex-col justify-between" style={{ flex: 1 }}>
          <div className="grid-2">
            <div className="metric-card">
              <h3>Volume</h3>
              <div className="flex items-center gap-2">
                <span className="metric-value text-cyan">{levelL ? levelL.toFixed(1) : '0.0'}</span>
                <span className="metric-unit">L ({levelPct ? levelPct.toFixed(0) : '0'}%)</span>
              </div>
            </div>
            <div className="metric-card">
              <h3>Flow Rate</h3>
              <div className="flex items-center gap-2">
                <span className="metric-value text-green">{flowLPM ? flowLPM.toFixed(2) : '0.00'}</span>
                <span className="metric-unit">L/min</span>
              </div>
            </div>
          </div>

          <div className="actuator-list mt-4">
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Droplets size={16}/> Main Valve</span>
              <div className={`status-badge ${mainValve ? 'connected' : 'disconnected'}`}>
                {mainValve ? 'OPEN' : 'CLOSED'}
              </div>
            </div>
            <div className="actuator-item">
              <span className="flex items-center gap-2 text-muted"><Activity size={16}/> Failsafe Valve</span>
              <div className={`status-badge ${fsValve ? 'disconnected' : 'connected'}`}>
                {fsValve ? 'OPEN (FAULT)' : 'CLOSED (SAFE)'}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Plant2Card;
