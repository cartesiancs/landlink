export type ConnectStepProps = {
  titleLines: readonly [string, string];
  showMedia?: boolean;
};

export function ConnectStep({
  titleLines,
  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  showMedia = true,
}: ConnectStepProps) {
  const [line1, line2] = titleLines;

  return (
    <div className="flex h-full flex-col px-4 pt-8 pb-4">
      <h2 className="text-3xl font-normal leading-tight tracking-tight">
        {line1}
        <br />
        {line2}
      </h2>

      <div className="flex-1" />

      {/* {showMedia ? (
        <div className="h-20 w-full rounded-md bg-muted" />
      ) : null} */}
    </div>
  );
}
