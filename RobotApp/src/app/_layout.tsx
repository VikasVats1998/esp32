import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { DarkTheme, DefaultTheme, ThemeProvider } from 'expo-router';
import { useColorScheme } from 'react-native';

import AppTabs from '@/components/app-tabs';
import { useConnectionNotes, useEventPump, useLogPump } from '@/hooks/use-robot';
import { installFeedback } from '@/lib/feedback';

const queryClient = new QueryClient();
installFeedback();

function Pumps() {
  useEventPump();
  useLogPump();
  useConnectionNotes();
  return null;
}

export default function RootLayout() {
  const colorScheme = useColorScheme();
  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider value={colorScheme === 'dark' ? DarkTheme : DefaultTheme}>
        <Pumps />
        <AppTabs />
      </ThemeProvider>
    </QueryClientProvider>
  );
}
