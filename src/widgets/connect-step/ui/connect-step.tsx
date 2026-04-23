export type ConnectStepProps = {
  titleLines: readonly [string, string];
};

export function ConnectStep({ titleLines }: ConnectStepProps) {
  const [line1, line2] = titleLines;

  return (
    <div className="flex h-full flex-col px-4 pt-8 pb-4">
      <h2 className="text-3xl font-normal leading-tight tracking-tight">
        {line1}
        <br />
        {line2}
      </h2>

      <div className="flex-1" />
    </div>
  );
}
