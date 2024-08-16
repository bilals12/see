// define colors for each event type
const eventColors = {
    leftclicks: 'pink',
    rightclicks: 'orange',
    keypresses: 'lightblue',
    mousemoves: 'lightgreen'
};

// fetch cumulative data and update the display
fetch('cumulative_data.csv')
    .then(response => response.text())
    .then(data => {
        const lines = data.split('\n').slice(1); // skip header row
        lines.forEach(line => {
            const [event, count] = line.split(',');
            const element = document.getElementById(event);
            if (element) {
                // correct text content without duplication
                element.textContent = `${count}`;  
                element.style.color = eventColors[event] || '#00FF00'; // use corresponding color from eventColors
                element.style.cursor = 'pointer';  // make text clickable
                
                // add event listener for click
                element.addEventListener('click', () => {
                    filterAndRender(event);
                });
            }
        });
    })
    .catch(error => console.error('error loading cumulative data:', error));

// fetch past 24h data + generate chart
fetch('past_24_hours_data.csv')
    .then(response => response.text())
    .then(data => {
        const parsedData = parseCSV(data);
        console.log('Parsed data:', parsedData);
        renderChart(parsedData);
    })
    .catch(error => console.error('error loading past 24 hours data:', error));

function parseCSV(data) {
    const lines = data.split('\n').slice(1); // skip header row
    const parsedData = lines.map(line => {
        const [timestamp, keypresses, mousemoves, leftclicks, rightclicks] = line.split(',');
        return {
            timestamp: new Date(Number(timestamp) * 1000), // convert unix timestamp to JS date
            keypresses: parseInt(keypresses, 10) || 0,
            mousemoves: mousemoves ? parseFloat(mousemoves.replace(' meters', '')) : 0,
            leftclicks: parseInt(leftclicks, 10) || 0,
            rightclicks: parseInt(rightclicks, 10) || 0
        };
    }).filter(d => !isNaN(d.timestamp.getTime()) && d.timestamp.getTime() > 0);

    return parsedData;
}

function renderChart(data, selectedMetric = null) {
    const margin = { top: 20, right: 80, bottom: 30, left: 50 },
          width = 800 - margin.left - margin.right,
          height = 400 - margin.top - margin.bottom;

    // clear existing SVG to prevent duplicates
    d3.select('#chart').selectAll('svg').remove();

    const svg = d3.select('#chart').append('svg')
        .attr('width', width + margin.left + margin.right)
        .attr('height', height + margin.top + margin.bottom)
      .append('g')
        .attr('transform', `translate(${margin.left},${margin.top})`);

    const x = d3.scaleTime().range([0, width]);
    const y = d3.scaleLinear().range([height, 0]);

    const startTime = d3.min(data, d => d.timestamp);
    const endTime = new Date(startTime.getTime() + 24 * 60 * 60 * 1000); // 24h from start

    x.domain([startTime, endTime]);

    let yMax;

    if (selectedMetric) {
        // calculate yMax based only on selected metric
        yMax = d3.max(data, d => d[selectedMetric]);
    } else {
        // calculate the maximum value for y-axis (incl. metrics)
        yMax = d3.max(data, d => Math.max(d.keypresses, d.leftclicks, d.rightclicks));
    }

    console.log('Calculated yMax:', yMax); // debugging: check max value calculation
    y.domain([0, yMax * 1.1]); // add 10% padding to top

    // create x-axis with 2h intervals
    const xAxis = d3.axisBottom(x)
        .ticks(d3.timeHour.every(2))
        .tickFormat(d3.timeFormat('%H:%M'));

    svg.append('g')
        .attr('transform', `translate(0,${height})`)
        .call(xAxis)
        .attr('color', '#00FF00');

    // Create y-axis
    const yAxis = d3.axisLeft(y)
        .ticks(10)
        .tickFormat(d => d % 1 === 0 ? d : d.toFixed(2));

    svg.append('g')
        .call(yAxis)
        .attr('color', '#00FF00');

    // add grid
    svg.append('g')
        .attr('class', 'grid')
        .attr('color', 'rgba(255, 255, 255, 0.1)')
        .call(d3.axisLeft(y)
            .ticks(10)
            .tickSize(-width)
            .tickFormat('')
        );

    // define line generators
    const line = d3.line()
        .curve(d3.curveMonotoneX)  // apply smoothing to line
        .x(d => x(d.timestamp))
        .y(d => y(d.value));

    // define metrics to plot
    const metrics = selectedMetric ? [selectedMetric] : ['keypresses', 'leftclicks', 'rightclicks'];

    // [lot lines for keypresses, leftclicks, rightclicks
    metrics.forEach(metric => {
        const metricData = data.map(d => ({timestamp: d.timestamp, value: d[metric]}));
        svg.append('path')
            .datum(metricData)
            .attr('fill', 'none')
            .attr('stroke', eventColors[metric])
            .attr('stroke-width', 2)
            .attr('d', line);
    });

    // tooltip
    const tooltip = d3.select('body').append('div')
        .attr('class', 'tooltip')
        .style('opacity', 0)
        .style('position', 'absolute')
        .style('background-color', 'rgba(0, 0, 0, 0.7)')
        .style('color', '#00FF00')
        .style('padding', '10px')
        .style('border-radius', '5px')
        .style('pointer-events', 'none');

    // vertical line for tooltip
    const tooltipLine = svg.append('line')
        .attr('class', 'tooltip-line')
        .attr('y1', 0)
        .attr('y2', height)
        .style('stroke', '#00FF00')
        .style('stroke-width', '1px')
        .style('stroke-dasharray', '3, 3')
        .style('opacity', 0);

    // overlay to capture mouse events
    svg.append('rect')
        .attr('width', width)
        .attr('height', height)
        .style('fill', 'none')
        .style('pointer-events', 'all')
        .on('mouseover', () => {
            tooltip.style('opacity', 1);
            tooltipLine.style('opacity', 1);
        })
        .on('mouseout', () => {
            tooltip.style('opacity', 0);
            tooltipLine.style('opacity', 0);
        })
        .on('mousemove', event => mousemove(event, data, selectedMetric));

    function mousemove(event, data, selectedMetric) {
        const bisect = d3.bisector(d => d.timestamp).left;
        const x0 = x.invert(d3.pointer(event)[0]);
        const i = bisect(data, x0, 1);
        
        if (i > 0 && i < data.length) {
            const d0 = data[i - 1];
            const d1 = data[i];
            const d = x0 - d0.timestamp > d1.timestamp - x0 ? d1 : d0;

            tooltipLine.attr('transform', `translate(${x(d.timestamp)}, 0)`);

            tooltip.html(`
                time: ${d3.timeFormat('%H:%M')(d.timestamp)}<br>
                keypresses: ${d.keypresses}<br>
                left clicks: ${d.leftclicks}<br>
                right clicks: ${d.rightclicks}
            `)
            .style('left', `${event.pageX + 10}px`)
            .style('top', `${event.pageY - 10}px`);
        }
    }
}

function filterAndRender(selectedMetric) {
    fetch('past_24_hours_data.csv')
        .then(response => response.text())
        .then(data => {
            const parsedData = parseCSV(data);
            renderChart(parsedData, selectedMetric);
        })
        .catch(error => console.error('error loading past 24 hours data:', error));
}
