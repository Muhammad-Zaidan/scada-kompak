import React from 'react';
import { AlertCircle, Terminal } from 'lucide-react';

const AlertLog = ({ alerts, mtuState }) => {
  return (
    <div className="glass-panel flex-col">
      <h2><Terminal className="text-yellow" /> MTU Status & Alerts</h2>
      
      <div className="metric-card mt-4 mb-4" style={{ background: 'rgba(245, 158, 11, 0.05)', borderColor: 'rgba(245, 158, 11, 0.2)' }}>
        <h3 className="text-yellow">pH Dosing State Machine</h3>
        <div className="metric-value text-yellow" style={{ fontSize: '1.5rem', letterSpacing: '2px' }}>
          {mtuState}
        </div>
      </div>

      <h3 className="mt-4"><AlertCircle size={14} style={{ display: 'inline', marginRight: '4px' }}/> System Log</h3>
      
      <div className="alert-list mt-2">
        {alerts.length === 0 ? (
          <div className="text-muted" style={{ padding: '1rem', textAlign: 'center', fontSize: '0.875rem' }}>
            No recent alerts. System is nominal.
          </div>
        ) : (
          alerts.map((alert, idx) => (
            <div key={idx} className={`alert-item ${alert.type === 'PH_FAULT' || alert.type === 'LEVEL_SENSOR_FAULT' ? 'fault' : 'dosing'}`}>
              <div className="alert-time">{alert.time}</div>
              <div>{alert.msg}</div>
            </div>
          ))
        )}
      </div>
    </div>
  );
};

export default AlertLog;
