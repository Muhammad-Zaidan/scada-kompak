import { createClient } from '@supabase/supabase-js';

const supabaseUrl = 'https://yrtrjdlgzixtnjwzsfjj.supabase.co';
const supabaseAnonKey = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlydHJqZGxneml4dG5qd3pzZmpqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzMyMDA2MzUsImV4cCI6MjA4ODc3NjYzNX0.GdQ2J2jxSQXdTB0fAbUiSvemnLJeCSU7W3geHPsBQ2g';

export const supabase = createClient(supabaseUrl, supabaseAnonKey);
